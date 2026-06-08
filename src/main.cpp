#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <opencv2/imgproc.hpp>

#include "camera/CameraSource.h"
#include "camera/MockSource.h"
#include "vision/HandDetector.h"
#include "vision/GestureClassifier.h"
#include "game/GameState.h"
#include "game/InstrumentMapper.h"
#include "game/InstrumentPool.h"
#include "audio/OscSender.h"
#include "server/HttpServer.h"
#include "server/WsServer.h"
#include "server/MjpegStreamer.h"
#include "util/Config.h"
#include "util/RingBuffer.h"
#include "Types.h"

#ifndef PLATFORM_MAC
#include "camera/ArducamSource.h"
#endif

static std::atomic<bool> g_running{true};

static void onSignal(int) { g_running = false; }

// Frame passed between camera thread and vision thread
struct Frame {
    CameraFrame cam;
};

int main(int argc, char* argv[]) {
    std::string config_path = "config.json";
    for (int i = 1; i < argc - 1; ++i)
        if (std::string(argv[i]) == "--config") config_path = argv[i+1];

    Config cfg = Config::load(config_path);

    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    // ---- Camera ----
    std::unique_ptr<CameraSource> cam;
#ifdef PLATFORM_MAC
    cam = std::make_unique<MockSource>(cfg.mock_video, cfg.camera_width, cfg.camera_height);
#else
    if (cfg.camera == "mock")
        cam = std::make_unique<MockSource>(cfg.mock_video, cfg.camera_width, cfg.camera_height);
    else
        cam = std::make_unique<ArducamSource>();
#endif
    if (!cam->open()) {
        std::cerr << "Failed to open camera\n";
        return 1;
    }

    // ---- Vision ----
    HandDetector     detector(cfg.yolo_model, cfg.yolo_conf, cfg.yolo_iou);
    GestureClassifier classifier;
    if (!detector.load()) {
        std::cerr << "Warning: YOLO model not loaded — hand detection disabled\n";
    }

    // ---- Game / audio ----
    GameState       game;
    InstrumentMapper mapper;
    InstrumentPool  pool;
    OscSender       osc(cfg.osc_host, cfg.osc_port);
    if (!osc.connect()) {
        std::cerr << "Warning: OSC connection failed — SuperCollider may not be running\n";
    }
    osc.connectTidal("127.0.0.1", 6010); // also send /ctrl to Tidal

    // ---- Servers ----
    // Assets dir: relative to binary (copied by CMake post-build)
    HttpServer    http("assets", cfg.http_port);
    WsServer      ws(cfg.ws_port);
    MjpegStreamer mjpeg(cfg.mjpeg_port);

    http.start();
    ws.start();
    mjpeg.start();

    std::cout << "Instrument running\n"
              << "  HTTP   → http://localhost:" << cfg.http_port  << "\n"
              << "  SSE    → http://localhost:" << cfg.ws_port    << "/state\n"
              << "  MJPEG  → http://localhost:" << cfg.mjpeg_port << "/stream.mjpeg\n"
              << "  OSC    → " << cfg.osc_host << ":" << cfg.osc_port << "\n";

    // ---- Frame ring buffer (camera → vision) ----
    RingBuffer<Frame, 4> frame_ring;

    // ---- Camera thread ----
    std::thread cam_thread([&]() {
        while (g_running) {
            Frame f;
            if (cam->nextFrame(f.cam, 200))
                frame_ring.push(std::move(f));
        }
    });

    // ---- Main loop: vision + game + broadcast ----
    auto t_last = std::chrono::steady_clock::now();
    HandList last_hands;

    while (g_running) {
        auto f = frame_ring.pop();
        if (!f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        auto now   = std::chrono::steady_clock::now();
        float dt   = std::chrono::duration<float>(now - t_last).count();
        t_last     = now;

        // Vision — use colour frame for detection if available (Mac webcam)
        auto detections = detector.detect(f->cam.confidence, f->cam.color);
        auto hands      = classifier.classify(detections,
                                               f->cam.depth,
                                               f->cam.confidence,
                                               cam->width(),
                                               cam->height(),
                                               f->cam.color);

        // Pool: detect hand arrivals and departures
        {
            static std::unordered_set<int> prevHandIds;
            std::unordered_set<int> curHandIds;
            for (const auto& h : hands) curHandIds.insert(h.id);
            // New hands
            for (int id : curHandIds)
                if (!prevHandIds.count(id)) pool.assign(id);
            // Lost hands
            for (int id : prevHandIds)
                if (!curHandIds.count(id)) pool.release(id);
            prevHandIds = curHandIds;
            pool.tick(dt);
        }

        // Game state
        const auto& state = game.tick(hands, dt);

        // Music params (global: tempo from avg X, pitch from avg Y, reverb from avg Z)
        auto music = mapper.map(hands, state);

        // OSC: per-hand depth-bucket messages + global params
        osc.send(hands, music, state);
        // OSC: pool assignments → Tidal /ctrl
        osc.sendPool(pool, hands, music);

        // MJPEG: hand pixels on black — only within detected bbox, hard luma key
        static int frame_count = 0;
        if (++frame_count % 2 == 0) {
            const cv::Mat& src = !f->cam.color.empty() ? f->cam.color : f->cam.confidence;
            cv::Mat out = cv::Mat::zeros(src.size(), src.type());

            if (!hands.empty() && src.type() == CV_8UC3) {
                int W = src.cols, H = src.rows;
                for (const auto& h : hands) {
                    // Tight bbox — just the hand area
                    float bw = h.bw * 1.1f, bh = h.bh * 1.1f;
                    int rx = std::max(0,    (int)((h.x - bw*0.5f) * W));
                    int ry = std::max(0,    (int)((h.y - bh*0.5f) * H));
                    int rw = std::min(W-rx, (int)(bw * W));
                    int rh = std::min(H-ry, (int)(bh * H));
                    if (rw <= 0 || rh <= 0) continue;

                    cv::Mat roi = src(cv::Rect(rx,ry,rw,rh));

                    // Luma key within bbox only
                    cv::Mat gray;
                    cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
                    // Adaptive threshold relative to roi max — handles varying light
                    double roiMax;
                    cv::minMaxLoc(gray, nullptr, &roiMax);
                    uchar thresh = (uchar)(roiMax * 0.45); // bottom 45% = background
                    cv::Mat key;
                    cv::threshold(gray, key, thresh, 255, cv::THRESH_BINARY);
                    cv::GaussianBlur(key, key, cv::Size(5,5), 1.5);

                    cv::Mat out_roi = out(cv::Rect(rx,ry,rw,rh));
                    for (int y = 0; y < rh; ++y) {
                        for (int x = 0; x < rw; ++x) {
                            float k = key.at<uchar>(y,x) / 255.f;
                            if (k > 0.1f) {
                                auto p = roi.at<cv::Vec3b>(y,x);
                                out_roi.at<cv::Vec3b>(y,x) = cv::Vec3b(
                                    (uchar)(p[0]*k),(uchar)(p[1]*k),(uchar)(p[2]*k));
                            }
                        }
                    }
                }
            }
            mjpeg.pushFrame(out);
        }

        // WebSocket: broadcast full state
        // Embed hands into state for JSON broadcast
        // (WsServer::broadcast takes GameStateData — extend it inline)
        ws.broadcast(state, hands);

        last_hands = hands;
    }

    cam->close();
    cam_thread.join();
    http.stop();
    ws.stop();
    mjpeg.stop();

    std::cout << "Shutdown complete\n";
    return 0;
}

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#ifdef HAVE_LIBLO
#include <lo/lo.h>
#else
typedef void* lo_message;
static lo_message lo_message_new() { return nullptr; }
static void lo_message_free(lo_message) {}
static void lo_send_message(void*, const char*, lo_message) {}
#endif
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

#include "camera/CameraSource.h"
#include "camera/MockSource.h"
#include "vision/HandDetector.h"
#include "vision/GestureClassifier.h"
#include "game/GameState.h"
#include "game/InstrumentMapper.h"
#include "game/InstrumentPool.h"
#include "audio/OscSender.h"
#include "audio/OscReceiver.h"
#include "server/HttpServer.h"
#include "server/WsServer.h"
#include "server/MjpegStreamer.h"
#include "util/Config.h"
#include "util/RingBuffer.h"
#include "Types.h"
#include "renderer/Renderer.h"

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
    osc.silenceAllOrbits();              // clear stale gains on startup

    // ---- OSC Receiver ----
    OscReceiver     osc_rx(57130);
    osc_rx.start();

    // ---- Servers ----
    // Assets dir: relative to binary (copied by CMake post-build)
    HttpServer    http("assets", cfg.http_port, &osc);
    WsServer      ws(cfg.ws_port);
    MjpegStreamer mjpeg(cfg.mjpeg_port);

    http.start();
    ws.start();
    mjpeg.start();

    // ---- Visualizer ----
    std::unique_ptr<Renderer> visualizer;
    if (cfg.enable_visualizer) {
        visualizer = std::make_unique<Renderer>();
        if (!visualizer->init(cfg.visualizer_width, cfg.visualizer_height, cfg.fullscreen, osc, cfg)) {
            std::cerr << "Warning: Failed to initialize visualizer\n";
            visualizer.reset();
        }
    }

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
    auto start_time = std::chrono::steady_clock::now();
    HandList last_hands;

    while (g_running) {
        if (visualizer && visualizer->shouldClose()) {
            g_running = false;
            break;
        }

        auto f = frame_ring.pop();

        auto now   = std::chrono::steady_clock::now();
        float dt   = std::chrono::duration<float>(now - t_last).count();
        t_last     = now;

        if (f) {
            // Vision — use colour frame for detection if available (Mac webcam)
            cv::Mat confidence_input = f->cam.confidence;
            if (f->cam.color.empty() && !f->cam.depth.empty()) {
                // Mask out background: only keep pixels with depth in [50, max_depth_mm] mm
                cv::Mat depth_mask = (f->cam.depth >= 50.0f) & (f->cam.depth <= cfg.max_depth_mm);
                confidence_input = f->cam.confidence.clone();
                confidence_input.setTo(0, ~depth_mask);
            }
            auto detections = detector.detect(confidence_input, f->cam.color);
            auto hands      = classifier.classify(detections,
                                                   f->cam.depth,
                                                   confidence_input,
                                                   cam->width(),
                                                   cam->height(),
                                                   f->cam.color);

            // Pool: debounced hand arrivals/departures
            // Require ARRIVE_FRAMES consecutive detections before assigning
            // Pool assignment with minimal debounce — prevents YOLO flicker from
            // cycling through instruments. Cursor updates immediately (uses raw hands).
            {
                static constexpr int STABLE_FRAMES = 5; // ~165ms at 30fps
                static std::unordered_map<int,int> stableCount;
                static std::unordered_set<int>     confirmed;

                std::unordered_set<int> curIds;
                for (const auto& h : hands) curIds.insert(h.id);

                // Increment stability counter, assign when stable
                for (int id : curIds) {
                    stableCount[id]++;
                    if (stableCount[id] >= STABLE_FRAMES && !confirmed.count(id)) {
                        confirmed.insert(id);
                        pool.assign(id);
                    }
                }
                // Clean up lost hands immediately
                for (auto it = stableCount.begin(); it != stableCount.end(); )
                    if (!curIds.count(it->first)) it = stableCount.erase(it); else ++it;

                bool wasEmpty = confirmed.empty();
                for (auto it = confirmed.begin(); it != confirmed.end(); ) {
                    if (!curIds.count(*it)) { pool.release(*it); it = confirmed.erase(it); }
                    else ++it;
                }
                // When all hands leave, immediately zero all gains
                if (!wasEmpty && confirmed.empty()) {
                    osc.silenceAllOrbits();
                }

                pool.tick(dt);

                // Hush all patterns after 15s with no confirmed hands
                static float silenceTimer = 0.f;
                static bool  hushed = false;
                bool& hushedRef = hushed; // accessible below for sendPool gate
                if (confirmed.empty()) {
                    silenceTimer += dt;
                    if (silenceTimer >= 15.f && !hushed) {
                        hushed = true;
#ifdef HAVE_LIBLO
                        if (osc.tidalAddr()) {
                            lo_address ta = (lo_address)osc.tidalAddr();
                            // Zero all orbit gains — d1-d12
                            for (int i = 0; i < 12; i++) {
                                std::string key = "o" + std::to_string(i) + "_gain";
                                lo_message m = lo_message_new();
                                lo_message_add_string(m, key.c_str());
                                lo_message_add_float(m, 0.0f);
                                lo_send_message(ta, "/ctrl", m);
                                lo_message_free(m);
                            }
                            // Also zero transformation_active — silences d15/d16
                            lo_message m2 = lo_message_new();
                            lo_message_add_string(m2, "transformation_active");
                            lo_message_add_float(m2, 0.0f);
                            lo_send_message(ta, "/ctrl", m2);
                            lo_message_free(m2);
                        }
#endif
                        std::cout << "[Pool] 15s silence — zeroing all gains\n";
                    }
                } else {
                    silenceTimer = 0.f;
                    if (hushed) {
                        hushed = false;
                        std::cout << "[Pool] hands returned — gains will restore via pool\n";
                    }
                }
            }

            // Game state
            const auto& state = game.tick(hands, dt);

            // Music params (global: tempo from avg X, pitch from avg Y, reverb from avg Z)
            auto music = mapper.map(hands, state);

            // OSC: per-hand depth-bucket messages + global params
            float elapsed_sec = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();
            osc.setElapsedTime(elapsed_sec);
            osc.send(hands, music, state, cfg.sc_instruments);
            // OSC: pool assignments → Tidal /ctrl
            osc.sendPool(pool, hands, music);

            // MJPEG: publish either the old processed hand-mask or a raw camera view.
            // On Pi/projector setups, "depth_color" gives the Arducam example-style
            // colourful 3D depth preview. For GL-native Pi use this stream is still
            // available, but the C++ visualizer is preferred when enabled.
            static int frame_count = 0;
            if (++frame_count % 2 == 0) {
                if (cfg.mjpeg_source == "confidence") {
                    mjpeg.pushFrame(f->cam.confidence);
                } else if (cfg.mjpeg_source == "depth") {
                    mjpeg.pushFrame(f->cam.depth);
                } else if (cfg.mjpeg_source == "depth_color") {
                    cv::Mat depth8, depth_color;
                    f->cam.depth.convertTo(depth8, CV_8U, 255.0 / 4000.0, 0);
                    cv::applyColorMap(depth8, depth_color, cv::COLORMAP_RAINBOW);
                    if (!f->cam.confidence.empty()) {
                        depth_color.setTo(cv::Scalar(0, 0, 0), f->cam.confidence < 30.0f);
                    }
                    mjpeg.pushFrame(depth_color);
                } else if (cfg.mjpeg_source == "color" && !f->cam.color.empty()) {
                    mjpeg.pushFrame(f->cam.color);
                } else {
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
            }

            // WebSocket: broadcast full state
            std::vector<int> current_seq;
            if (visualizer) {
                current_seq = visualizer->getSequence();
            }
            auto spawns = osc_rx.popSpawns();
            ws.broadcast(state, hands, cfg.mirror_x,
                         cfg.coord_x_min, cfg.coord_x_max,
                         cfg.coord_y_min, cfg.coord_y_max,
                         cfg.coord_z_min, cfg.coord_z_max,
                         cfg.show_hints, cfg.show_preview_history,
                         current_seq, spawns);

            last_hands = hands;
        }

        if (visualizer) {
            visualizer->render(last_hands, dt);
        } else {
            if (!f) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    cam->close();
    cam_thread.join();
    if (visualizer) {
        visualizer->close();
    }
    osc_rx.stop();
    http.stop();
    ws.stop();
    mjpeg.stop();

    std::cout << "Shutdown complete\n";
    return 0;
}

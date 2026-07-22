#include "HttpServer.h"
#include "../audio/OscSender.h"
#include <httplib.h>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <array>
#include <memory>
#include <csignal>
#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
#else
#  include "../../third_party/json.hpp"
#endif

#ifdef HAVE_LIBLO
#  include <lo/lo.h>
#endif

namespace {
class TalkReceiver {
public:
    void start() {
        std::lock_guard<std::mutex> lk(mutex_);
        stopLocked();
        // Browser sends MediaRecorder audio/webm;codecs=opus chunks. ffmpeg decodes
        // stdin directly to ALSA default output (Pi headphones/earphones). This is
        // more reliable in a boot service than ffplay, which depends on SDL/Pulse.
        pipe_ = popen("ffmpeg -hide_banner -loglevel warning -fflags nobuffer -flags low_delay -i pipe:0 -vn -ac 1 -ar 48000 -f alsa default", "w");
        if (!pipe_) std::cerr << "[TalkReceiver] failed to start ffmpeg; install ffmpeg and configure ALSA default output\n";
        else        std::cout << "[TalkReceiver] listening: projector mic → Pi audio out\n";
    }

    bool chunk(const std::string& bytes) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!pipe_ || bytes.empty()) return false;
        size_t written = fwrite(bytes.data(), 1, bytes.size(), pipe_);
        if (written != bytes.size() || fflush(pipe_) == EOF || ferror(pipe_)) {
            std::cerr << "[TalkReceiver] ffmpeg audio pipe broke; stopping receiver\n";
            stopLocked();
            return false;
        }
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lk(mutex_);
        stopLocked();
    }

private:
    void stopLocked() {
        if (!pipe_) return;
        pclose(pipe_);
        pipe_ = nullptr;
        std::cout << "[TalkReceiver] stopped\n";
    }

    std::mutex mutex_;
    FILE* pipe_ = nullptr;
};

TalkReceiver g_talk;

struct PiMicStream {
    explicit PiMicStream(FILE* p) : pipe(p) {}
    ~PiMicStream() { if (pipe) pclose(pipe); }
    FILE* pipe = nullptr;
    std::array<char, 4096> buf{};
};

std::shared_ptr<PiMicStream> openPiMicStream() {
    // Captures the Pi's default ALSA microphone, encodes Opus/WebM, and exposes
    // it as a browser-playable stream. If needed, set the default input with
    // raspi-config / ALSA, or replace "default" with a device like "hw:1,0".
    FILE* p = popen("ffmpeg -hide_banner -loglevel warning -f alsa -i default -ac 1 -ar 48000 -c:a libopus -b:a 32k -application voip -fflags nobuffer -flags low_delay -f webm pipe:1", "r");
    if (!p) std::cerr << "[PiMicStream] failed to start ffmpeg; install ffmpeg and connect a mic\n";
    else    std::cout << "[PiMicStream] streaming Pi mic → projector speaker\n";
    return std::make_shared<PiMicStream>(p);
}
}

HttpServer::HttpServer(const std::string& assets_dir, int port, OscSender* osc)
    : assets_dir_(assets_dir), port_(port), osc_(osc) {}

void HttpServer::start() {
    running_ = true;
    thread_ = std::thread([this]() {
        // If ffmpeg/ffplay exits while we write to its pipe, do not let SIGPIPE
        // terminate the whole instrument process. Report failure to the browser so
        // it can reconnect instead.
        std::signal(SIGPIPE, SIG_IGN);

        httplib::Server svr;
        svr.new_task_queue = [] { return new httplib::ThreadPool(12); };
        svr.set_mount_point("/", assets_dir_.c_str());

        // /talk/* — projector browser microphone → Pi default audio output.
        // Requires ffplay on the Pi: sudo apt install ffmpeg
        svr.Post("/talk/start", [](const httplib::Request&, httplib::Response& res) {
            g_talk.start();
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });
        svr.Post("/talk/chunk", [](const httplib::Request& req, httplib::Response& res) {
            if (g_talk.chunk(req.body)) {
                res.set_content("ok", "text/plain");
            } else {
                res.status = 503;
                res.set_content("audio receiver unavailable", "text/plain");
            }
            res.set_header("Access-Control-Allow-Origin", "*");
        });
        svr.Post("/talk/stop", [](const httplib::Request&, httplib::Response& res) {
            g_talk.stop();
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });
        svr.Options(R"(/talk/.*)", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        // /pi-mic — Pi microphone → projector browser speaker.
        // Requires ffmpeg and a default ALSA capture device on the Pi.
        svr.Get("/pi-mic", [](const httplib::Request&, httplib::Response& res) {
            auto mic = openPiMicStream();
            if (!mic || !mic->pipe) {
                res.status = 503;
                res.set_content("pi mic unavailable", "text/plain");
                res.set_header("Access-Control-Allow-Origin", "*");
                return;
            }
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Cache-Control", "no-cache");
            res.set_chunked_content_provider("audio/webm", [mic](size_t, httplib::DataSink& sink) {
                if (!mic->pipe) return false;
                size_t n = fread(mic->buf.data(), 1, mic->buf.size(), mic->pipe);
                if (n == 0) return false;
                return sink.write(mic->buf.data(), n);
            });
        });

        // /orb POST — game events from browser → OSC
        // Handles: offering game orbs, transformation station /ctrl messages
        svr.Post("/orb", [this](const httplib::Request& req, httplib::Response& res) {
#ifdef HAVE_LIBLO
            try {
                auto j = nlohmann::json::parse(req.body);

                if (j.contains("ctrl")) {
                    // Send /ctrl to Tidal on port 6010
                    std::string key = j["ctrl"];
                    
                    // Intercept reflex controls for backend tempo bypassing
                    if (key == "reflex_active" && osc_) {
                        osc_->setReflexActive(j["value"].get<double>() > 0.5);
                    } else if (key == "reflex_cps" && osc_) {
                        osc_->setReflexCps((float)j["value"].get<double>());
                    }

                    lo_address tidal = lo_address_new("127.0.0.1", "6010");
                    lo_message m = lo_message_new();
                    lo_message_add_string(m, key.c_str());
                    if (j["value"].is_string()) {
                        lo_message_add_string(m, j["value"].get<std::string>().c_str());
                    } else {
                        lo_message_add_float(m, (float)j["value"].get<double>());
                    }
                    lo_send_message(tidal, "/ctrl", m);
                    lo_message_free(m);
                    lo_address_free(tidal);
                } else {
                    // Offering game: send to SuperCollider
                    int  orbId = j.value("orb",  -1);
                    int  held  = j.value("held",  0);
                    lo_address sc = lo_address_new("127.0.0.1", "57120");
                    if (orbId == -1) {
                        lo_message m = lo_message_new();
                        lo_message_add_int32(m, 1);
                        lo_send_message(sc, "/orb_climax", m);
                        lo_message_free(m);
                    } else {
                        lo_message m = lo_message_new();
                        lo_message_add_int32(m, orbId);
                        lo_message_add_int32(m, held);
                        lo_send_message(sc, "/orb", m);
                        lo_message_free(m);
                    }
                    lo_address_free(sc);
                }
            } catch (...) {}
#endif
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        // CORS preflight for /orb
        svr.Options("/orb", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        // /note POST — play a single one-shot note directly in SC (bypasses Tidal)
        // Body: { "midi": 60, "amp": 0.7, "decay": 0.8 }
        svr.Post("/note", [](const httplib::Request& req, httplib::Response& res) {
#ifdef HAVE_LIBLO
            try {
                auto j = nlohmann::json::parse(req.body);
                int   midi  = j.value("midi",  60);
                float amp   = j.value("amp",   0.7);
                float decay = j.value("decay", 0.8);
                // Send via SuperDirt /dirt/play — reliable, already working
                lo_address sc = lo_address_new("127.0.0.1", "57120");
                lo_message m = lo_message_new();
                lo_message_add_string(m, "cps");      lo_message_add_float(m, 0.5f);
                lo_message_add_string(m, "cycle");    lo_message_add_float(m, 0.f);
                lo_message_add_string(m, "delta");    lo_message_add_float(m, 0.5f);
                lo_message_add_string(m, "orbit");    lo_message_add_int32(m, 0);
                lo_message_add_string(m, "s");        lo_message_add_string(m, "superpiano");
                lo_message_add_string(m, "note");     lo_message_add_float(m, (float)(midi - 60));
                lo_message_add_string(m, "gain");     lo_message_add_float(m, amp);
                lo_message_add_string(m, "sustain");  lo_message_add_float(m, decay);
                lo_message_add_string(m, "room");     lo_message_add_float(m, 0.4f);
                lo_send_message(sc, "/dirt/play", m);
                lo_message_free(m);
                lo_address_free(sc);
            } catch (...) {}
#endif
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        svr.Options("/note", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        // /mute POST — mute/unmute hand instruments
        svr.Post("/mute", [this](const httplib::Request& req, httplib::Response& res) {
#ifdef HAVE_LIBLO
            try {
                auto j = nlohmann::json::parse(req.body);
                bool muted = j.value("muted", false);
                if (osc_) {
                    osc_->setPreviewMute(muted);
                } else {
                    lo_address sc = lo_address_new("127.0.0.1", "57120");
                    lo_message m = lo_message_new();
                    lo_message_add_int32(m, muted ? 1 : 0);
                    lo_send_message(sc, "/mute", m);
                    lo_message_free(m);
                    lo_address_free(sc);
                }
            } catch (...) {}
#endif
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        svr.Options("/mute", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        // /target POST — active target node index from browser
        svr.Post("/target", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = nlohmann::json::parse(req.body);
                int target = j.value("target", -1);
                if (osc_) {
                    osc_->setTargetNode(target);
                }
            } catch (...) {}
            res.set_content("ok", "text/plain");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        svr.Options("/target", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin",  "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        std::cout << "[HttpServer] serving " << assets_dir_ << " on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void HttpServer::stop() {
    if (thread_.joinable()) thread_.detach();
}

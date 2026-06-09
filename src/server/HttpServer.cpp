#include "HttpServer.h"
#include <httplib.h>
#include <iostream>
#include <cmath>
#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
#else
#  include "../../third_party/json.hpp"
#endif

#ifdef HAVE_LIBLO
#  include <lo/lo.h>
#endif

HttpServer::HttpServer(const std::string& assets_dir, int port)
    : assets_dir_(assets_dir), port_(port) {}

void HttpServer::start() {
    running_ = true;
    thread_ = std::thread([this]() {
        httplib::Server svr;
        svr.set_mount_point("/", assets_dir_.c_str());

        // /orb POST — game events from browser → OSC
        // Handles: offering game orbs, transformation station /ctrl messages
        svr.Post("/orb", [](const httplib::Request& req, httplib::Response& res) {
#ifdef HAVE_LIBLO
            try {
                auto j = nlohmann::json::parse(req.body);

                if (j.contains("ctrl")) {
                    // Transformation Station: send /ctrl to Tidal on port 6010
                    std::string key = j["ctrl"];
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

        std::cout << "[HttpServer] serving " << assets_dir_ << " on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void HttpServer::stop() {
    if (thread_.joinable()) thread_.detach();
}

#include "HttpServer.h"
#include <httplib.h>
#include <iostream>
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

        // /orb POST — game events from browser → OSC to SuperCollider
        svr.Post("/orb", [](const httplib::Request& req, httplib::Response& res) {
#ifdef HAVE_LIBLO
            try {
                auto j = nlohmann::json::parse(req.body);
                int  orbId = j.value("orb",  -1);
                int  held  = j.value("held",  0);

                lo_address sc = lo_address_new("127.0.0.1", "57120");
                if (orbId == -1) {
                    // Climax — all orbs held
                    lo_send(sc, "/orb_climax", "i", 1);
                } else {
                    lo_send(sc, "/orb", "ii", orbId, held);
                }
                lo_address_free(sc);
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

        std::cout << "[HttpServer] serving " << assets_dir_ << " on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void HttpServer::stop() {
    if (thread_.joinable()) thread_.detach();
}

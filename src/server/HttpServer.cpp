#include "HttpServer.h"
#include <httplib.h>
#include <iostream>

HttpServer::HttpServer(const std::string& assets_dir, int port)
    : assets_dir_(assets_dir), port_(port) {}

void HttpServer::start() {
    running_ = true;
    thread_ = std::thread([this]() {
        httplib::Server svr;
        svr.set_mount_point("/", assets_dir_.c_str());
        std::cout << "[HttpServer] serving " << assets_dir_ << " on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void HttpServer::stop() {
    // cpp-httplib server is stopped when thread exits; for now just detach
    if (thread_.joinable()) thread_.detach();
}

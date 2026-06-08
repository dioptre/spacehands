#pragma once
#include <string>
#include <thread>

// Serves the assets/ directory statically via cpp-httplib.
class HttpServer {
public:
    HttpServer(const std::string& assets_dir, int port);
    void start(); // non-blocking, spawns thread
    void stop();

private:
    std::string  assets_dir_;
    int          port_;
    std::thread  thread_;
    bool         running_ = false;
};

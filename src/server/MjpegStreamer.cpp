#include "MjpegStreamer.h"
#include <httplib.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

MjpegStreamer::MjpegStreamer(int port) : port_(port) {}

void MjpegStreamer::pushFrame(const cv::Mat& frame) {
    if (frame.empty()) return;
    cv::Mat display;
    if (frame.type() == CV_32F) {
        // ToF confidence/depth frames — normalize current raw range to grayscale.
        // This preserves a visible raw camera view for projector streaming whether
        // the source is confidence (usually 0-1024) or depth (millimetres).
        double minv = 0.0, maxv = 0.0;
        cv::minMaxLoc(frame, &minv, &maxv);
        if (std::isfinite(minv) && std::isfinite(maxv) && maxv > minv) {
            frame.convertTo(display, CV_8U, 255.0 / (maxv - minv), -minv * 255.0 / (maxv - minv));
        } else {
            frame.convertTo(display, CV_8U, 255.f / 1024.f);
        }
    } else if (frame.type() == CV_8UC3) {
        // Colour/depth-colormap BGR frame — send as-is. Brightness boosting would
        // distort the Arducam example-style rainbow depth preview.
        display = frame.clone();
    } else {
        display = frame.clone();
    }

    // Upscale the small ToF frame before JPEG encoding so the browser/projector
    // receives a smoother image instead of magnifying a 240x180 JPEG directly.
    if (display.cols > 0 && display.cols < 960) {
        cv::Mat up;
        double scale = 960.0 / display.cols;
        cv::resize(display, up, cv::Size(), scale, scale, cv::INTER_CUBIC);
        cv::GaussianBlur(up, up, cv::Size(3,3), 0.35);
        display = std::move(up);
    }

    // Encode to JPEG
    std::vector<uchar> buf;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 90};
    cv::imencode(".jpg", display, buf, params);

    std::lock_guard<std::mutex> lk(mutex_);
    jpeg_buf_  = std::move(buf);
    has_frame_ = true;
}

void MjpegStreamer::start() {
    thread_ = std::thread([this]() {
        httplib::Server svr;

        svr.Get("/stream.mjpeg", [this](const httplib::Request&, httplib::Response& res) {
            // httplib doesn't natively support multipart streaming; serve the
            // latest JPEG as a single image. Chromium will re-fetch via setInterval
            // in ws_client.js to create a pseudo-stream effect.
            std::vector<uchar> buf;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                if (!has_frame_) {
                    res.status = 204;
                    return;
                }
                buf = jpeg_buf_;
            }
            res.set_content(reinterpret_cast<const char*>(buf.data()), buf.size(), "image/jpeg");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        std::cout << "[MjpegStreamer] serving /stream.mjpeg on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void MjpegStreamer::stop() {
    if (thread_.joinable()) thread_.detach();
}

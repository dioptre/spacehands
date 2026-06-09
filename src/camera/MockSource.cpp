#include "MockSource.h"
#include <cmath>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <chrono>

MockSource::MockSource(const std::string& video_path, int w, int h)
    : path_(video_path), w_(w), h_(h) {}

bool MockSource::open() {
    if (!path_.empty()) {
        cap_.open(path_);
        if (!cap_.isOpened()) {
            std::cerr << "[MockSource] could not open " << path_ << "\n";
            path_.clear();
        } else {
            std::cout << "[MockSource] opened video file: " << path_ << "\n";
            return true;
        }
    }
    cap_.open(0);
    if (cap_.isOpened())
        std::cout << "[MockSource] webcam open\n";
    else
        std::cerr << "[MockSource] no webcam\n";
    return true;
}

bool MockSource::close() {
    if (cap_.isOpened()) cap_.release();
    return true;
}

static bool readFrame(cv::VideoCapture& cap, cv::Mat& out) {
    if (!cap.isOpened()) return false;
    cv::Mat bgr;
    try { if (!cap.read(bgr) || bgr.empty()) return false; } catch (...) { return false; }
    out = bgr;
    return true;
}

bool MockSource::nextFrame(CameraFrame& out, int /*timeout_ms*/) {
    std::this_thread::sleep_for(std::chrono::milliseconds(33));

    cv::Mat bgr;
    if (readFrame(cap_, bgr)) {
        // Full resolution colour frame for MJPEG (better quality)
        out.color = bgr.clone();
        // Downscaled for YOLO + confidence
        cv::Mat resized, gray;
        cv::resize(bgr, resized, cv::Size(w_, h_));
        cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
        gray.convertTo(out.confidence, CV_32F, 1024.f / 255.f);
        fail_count_ = 0;
    } else {
        if (++fail_count_ > 5 && cap_.isOpened()) {
            cap_.release();
            std::cerr << "[MockSource] webcam lost\n";
        }
        out.confidence = cv::Mat(h_, w_, CV_32F, cv::Scalar(0.f));
    }

    // Flat depth on Mac — no real ToF data, keep Z stable at ~300mm (near bucket)
    // so the cursor doesn't flip between near/far layers unpredictably
    out.depth = cv::Mat(h_, w_, CV_32F, cv::Scalar(300.f));
    ++frame_idx_;
    return true;
}

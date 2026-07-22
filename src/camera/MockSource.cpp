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
        
        // Generate an animated mock silhouette so the edge outline shader has something to trace!
        cv::Mat mock_color = cv::Mat::zeros(h_, w_, CV_8UC3);
        
        // Draw head
        float pulse = std::sin(frame_idx_ * 0.05f) * 5.0f;
        cv::circle(mock_color, cv::Point(w_ / 2, h_ / 3 + (int)pulse), 25, cv::Scalar(180, 50, 220), -1);
        
        // Draw body
        cv::ellipse(mock_color, cv::Point(w_ / 2, h_ * 2 / 3), cv::Size(30, 45), 0, 0, 360, cv::Scalar(180, 50, 220), -1);
        
        // Draw moving arms
        float arm_y = std::sin(frame_idx_ * 0.1f) * 15.0f;
        cv::line(mock_color, cv::Point(w_ / 2 - 30, h_ * 2 / 3 - 10), cv::Point(w_ / 2 - 70, h_ * 2 / 3 + (int)arm_y), cv::Scalar(180, 50, 220), 8);
        cv::line(mock_color, cv::Point(w_ / 2 + 30, h_ * 2 / 3 - 10), cv::Point(w_ / 2 + 70, h_ * 2 / 3 - (int)arm_y), cv::Scalar(180, 50, 220), 8);

        out.color = mock_color;
        
        cv::Mat gray;
        cv::cvtColor(mock_color, gray, cv::COLOR_BGR2GRAY);
        gray.convertTo(out.confidence, CV_32F, 1024.f / 255.f);
    }

    // 1300mm → z = 1 - (1300-200)/1800 = 1 - 0.611 = 0.389 → near bucket (< 0.5)
    out.depth = cv::Mat(h_, w_, CV_32F, cv::Scalar(1300.f));
    ++frame_idx_;
    return true;
}

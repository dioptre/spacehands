#include "HandDetector.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>

HandDetector::HandDetector(const std::string& model_path, float conf_thresh, float iou_thresh)
    : model_path_(model_path), conf_thresh_(conf_thresh), iou_thresh_(iou_thresh) {}

bool HandDetector::load() {
    try {
        net_ = cv::dnn::readNetFromONNX(model_path_);
    } catch (const std::exception& e) {
        std::cerr << "[HandDetector] failed to load: " << e.what() << "\n";
        return false;
    }
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    cv::setNumThreads(4); // use all available cores for inference
    loaded_ = true;
    std::cout << "[HandDetector] hand model loaded: " << model_path_ << "\n";
    return true;
}

cv::Mat HandDetector::preprocess(const cv::Mat& src, float& scale, int& pad_x, int& pad_y) {
    cv::Mat input8;
    if (src.type() == CV_8UC3) {
        input8 = src;
    } else {
        cv::Mat gray8;
        src.convertTo(gray8, CV_8U, 255.f / 1024.f);
        cv::cvtColor(gray8, input8, cv::COLOR_GRAY2BGR);
    }

    float sw = (float)INPUT_SIZE / input8.cols;
    float sh = (float)INPUT_SIZE / input8.rows;
    scale = std::min(sw, sh);
    int new_w = (int)(input8.cols * scale);
    int new_h = (int)(input8.rows * scale);
    pad_x = (INPUT_SIZE - new_w) / 2;
    pad_y = (INPUT_SIZE - new_h) / 2;

    cv::Mat resized;
    cv::resize(input8, resized, cv::Size(new_w, new_h));
    cv::Mat padded(INPUT_SIZE, INPUT_SIZE, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(pad_x, pad_y, new_w, new_h)));
    return padded;
}

std::vector<Detection> HandDetector::detect(const cv::Mat& confidence_frame,
                                             const cv::Mat& color_frame) {
    if (!loaded_) return {};

    float scale; int pad_x, pad_y;
    const cv::Mat& src = (!color_frame.empty()) ? color_frame : confidence_frame;
    cv::Mat input = preprocess(src, scale, pad_x, pad_y);

    cv::Mat blob;
    cv::dnn::blobFromImage(input, blob, 1.0 / 255.0,
                            cv::Size(INPUT_SIZE, INPUT_SIZE),
                            cv::Scalar(0, 0, 0), true, false, CV_32F);
    net_.setInput(blob);
    cv::Mat raw = net_.forward("output0");

    // YOLOv8 exported ONNX: [1, 5, num_anchors]
    // channels: cx, cy, w, h, conf  (already sigmoid, single class)
    // Reshape to [num_anchors, 5]
    cv::Mat out;
    if (raw.dims == 3) {
        // [1, 5, 2100] → transpose to [1, 2100, 5] then reshape
        // Use ptr access directly
    }

    int num_anchors = raw.size[2];
    int num_ch      = raw.size[1]; // 5

    std::vector<cv::Rect2f> boxes;
    std::vector<float>      scores;

    for (int i = 0; i < num_anchors; ++i) {
        float conf = raw.ptr<float>(0, 4)[i]; // channel 4 = confidence
        if (conf < conf_thresh_) continue;

        float cx = raw.ptr<float>(0, 0)[i]; // in INPUT_SIZE pixel space
        float cy = raw.ptr<float>(0, 1)[i];
        float bw = raw.ptr<float>(0, 2)[i];
        float bh = raw.ptr<float>(0, 3)[i];

        // Remove letterbox padding and normalise to [0,1]
        float x1 = (cx - bw * 0.5f - pad_x) / (INPUT_SIZE - 2.f * pad_x);
        float y1 = (cy - bh * 0.5f - pad_y) / (INPUT_SIZE - 2.f * pad_y);
        float x2 = (cx + bw * 0.5f - pad_x) / (INPUT_SIZE - 2.f * pad_x);
        float y2 = (cy + bh * 0.5f - pad_y) / (INPUT_SIZE - 2.f * pad_y);
        x1 = std::clamp(x1, 0.f, 1.f);
        y1 = std::clamp(y1, 0.f, 1.f);
        x2 = std::clamp(x2, 0.f, 1.f);
        y2 = std::clamp(y2, 0.f, 1.f);
        if (x2 - x1 < 0.01f || y2 - y1 < 0.01f) continue;

        boxes.push_back({x1, y1, x2 - x1, y2 - y1});
        scores.push_back(conf);
    }

    // NMS
    std::vector<int> idx(boxes.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b){ return scores[a] > scores[b]; });
    std::vector<bool> suppressed(idx.size(), false);
    std::vector<int>  keep;
    for (size_t i = 0; i < idx.size(); ++i) {
        if (suppressed[i]) continue;
        keep.push_back(idx[i]);
        const auto& a = boxes[idx[i]];
        for (size_t j = i + 1; j < idx.size(); ++j) {
            if (suppressed[j]) continue;
            const auto& b = boxes[idx[j]];
            auto inter = (a & b);
            if (inter.area() <= 0) continue;
            float iou = inter.area() / (a.area() + b.area() - inter.area() + 1e-6f);
            if (iou > iou_thresh_) suppressed[j] = true;
        }
    }

    // Live debug every 2s
    static auto last_print = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - last_print).count() >= 2.f) {
        last_print = now;
        float maxc = 0;
        for (int i = 0; i < num_anchors; ++i)
            maxc = std::max(maxc, raw.ptr<float>(0,4)[i]);
        std::cout << "[detect] max_conf=" << std::fixed << std::setprecision(3) << maxc
                  << " above_thresh=" << scores.size()
                  << " after_nms=" << keep.size() << "\n";
    }

    std::vector<Detection> result;
    for (int k : keep)
        result.push_back({boxes[k], scores[k]});
    return result;
}

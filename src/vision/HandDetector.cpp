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
        net_ = cv::dnn::readNet(model_path_);
    } catch (const std::exception& e) {
        std::cerr << "[HandDetector] failed to load: " << e.what() << "\n";
        return false;
    }
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    cv::setNumThreads(4); // use all available cores for inference
    
    if (model_path_.find("yolox") != std::string::npos) {
        input_w_ = 320;
        input_h_ = 192;
        is_yolox_ = true;
    } else if (model_path_.find("w8a8") != std::string::npos) {
        input_w_ = 640;
        input_h_ = 640;
        is_yolox_ = false;
    } else {
        input_w_ = 320;
        input_h_ = 320;
        is_yolox_ = false;
    }
    
    loaded_ = true;
    std::cout << "[HandDetector] hand model loaded: " << model_path_
              << " (" << input_w_ << "x" << input_h_ << ", YOLOX=" << is_yolox_ << ")\n";
    return true;
}

cv::Mat HandDetector::preprocess(const cv::Mat& src, float& scale, int& pad_x, int& pad_y) {
    cv::Mat input8;
    if (src.type() == CV_8UC3) {
        input8 = src;
    } else {
        cv::Mat normalized;
        cv::normalize(src, normalized, 0, 255, cv::NORM_MINMAX, CV_8U);
        cv::cvtColor(normalized, input8, cv::COLOR_GRAY2BGR);
    }

    float sw = (float)input_w_ / input8.cols;
    float sh = (float)input_h_ / input8.rows;
    scale = std::min(sw, sh);
    int new_w = (int)(input8.cols * scale);
    int new_h = (int)(input8.rows * scale);
    pad_x = (input_w_ - new_w) / 2;
    pad_y = (input_h_ - new_h) / 2;

    cv::Mat resized;
    cv::resize(input8, resized, cv::Size(new_w, new_h));
    cv::Mat padded(input_h_, input_w_, CV_8UC3, cv::Scalar(114, 114, 114));
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
    double scale_factor = is_yolox_ ? 1.0 : (1.0 / 255.0);
    cv::dnn::blobFromImage(input, blob, scale_factor,
                            cv::Size(input_w_, input_h_),
                            cv::Scalar(0, 0, 0), true, false, CV_32F);
    net_.setInput(blob);
    
    std::string out_layer = is_yolox_ ? "output" : "output0";
    cv::Mat raw = net_.forward(out_layer);

    std::vector<cv::Rect2f> boxes;
    std::vector<float>      scores;

    if (is_yolox_) {
        // raw shape: [1, 1260, 8]
        int num_anchors = raw.size[1];
        for (int i = 0; i < num_anchors; ++i) {
            float* row = raw.ptr<float>(0, i);
            float obj_conf = row[4];
            float cls2_conf = row[7]; // class 2 = hand
            float conf = obj_conf * cls2_conf;
            if (conf < conf_thresh_) continue;

            float cx = row[0];
            float cy = row[1];
            float bw = row[2];
            float bh = row[3];

            // Remove letterbox padding and normalise to [0,1]
            float x1 = (cx - bw * 0.5f - pad_x) / (input_w_ - 2.f * pad_x);
            float y1 = (cy - bh * 0.5f - pad_y) / (input_h_ - 2.f * pad_y);
            float x2 = (cx + bw * 0.5f - pad_x) / (input_w_ - 2.f * pad_x);
            float y2 = (cy + bh * 0.5f - pad_y) / (input_h_ - 2.f * pad_y);
            x1 = std::clamp(x1, 0.f, 1.f);
            y1 = std::clamp(y1, 0.f, 1.f);
            x2 = std::clamp(x2, 0.f, 1.f);
            y2 = std::clamp(y2, 0.f, 1.f);
            if (x2 - x1 < 0.01f || y2 - y1 < 0.01f) continue;

            boxes.push_back({x1, y1, x2 - x1, y2 - y1});
            scores.push_back(conf);
        }
    } else {
        // raw shape: [1, 4 + nc, num_anchors] (YOLOv8)
        int num_anchors = raw.size[2];
        int num_classes = raw.size[1] - 4;
        for (int i = 0; i < num_anchors; ++i) {
            float conf = 0.0f;
            for (int c = 0; c < num_classes; ++c) {
                float score = raw.ptr<float>(0, 4 + c)[i];
                if (score > conf) conf = score;
            }
            if (conf < conf_thresh_) continue;

            float cx = raw.ptr<float>(0, 0)[i];
            float cy = raw.ptr<float>(0, 1)[i];
            float bw = raw.ptr<float>(0, 2)[i];
            float bh = raw.ptr<float>(0, 3)[i];

            float x1 = (cx - bw * 0.5f - pad_x) / (input_w_ - 2.f * pad_x);
            float y1 = (cy - bh * 0.5f - pad_y) / (input_h_ - 2.f * pad_y);
            float x2 = (cx + bw * 0.5f - pad_x) / (input_w_ - 2.f * pad_x);
            float y2 = (cy + bh * 0.5f - pad_y) / (input_h_ - 2.f * pad_y);
            x1 = std::clamp(x1, 0.f, 1.f);
            y1 = std::clamp(y1, 0.f, 1.f);
            x2 = std::clamp(x2, 0.f, 1.f);
            y2 = std::clamp(y2, 0.f, 1.f);
            if (x2 - x1 < 0.01f || y2 - y1 < 0.01f) continue;

            boxes.push_back({x1, y1, x2 - x1, y2 - y1});
            scores.push_back(conf);
        }
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
        float max_obj = 0, max_c0 = 0, max_c1 = 0, max_c2 = 0;
        float max_comb0 = 0, max_comb1 = 0, max_comb2 = 0;
        if (is_yolox_) {
            for (int i = 0; i < raw.size[1]; ++i)
                maxc = std::max(maxc, raw.ptr<float>(0, i)[4] * raw.ptr<float>(0, i)[7]);
        } else {
            int num_classes = raw.size[1] - 4;
            for (int i = 0; i < raw.size[2]; ++i) {
                for (int c = 0; c < num_classes; ++c) {
                    float val = raw.ptr<float>(0, 4 + c)[i];
                    maxc = std::max(maxc, val);
                    max_obj = std::max(max_obj, val);
                    if (c == 0) { max_c0 = std::max(max_c0, val); max_comb0 = std::max(max_comb0, val); }
                    if (c == 1) { max_c1 = std::max(max_c1, val); max_comb1 = std::max(max_comb1, val); }
                    if (c == 2) { max_c2 = std::max(max_c2, val); max_comb2 = std::max(max_comb2, val); }
                }
            }
        }
        std::string src_info = "";
        if (src.type() == CV_32F) {
            double min_val = 0, max_val = 0;
            cv::minMaxLoc(src, &min_val, &max_val);
            src_info = " src_range=[" + std::to_string((int)min_val) + "," + std::to_string((int)max_val) + "]";
        }
        std::string shape_info = " shape=[";
        for (int d = 0; d < raw.dims; d++) {
            shape_info += std::to_string(raw.size[d]) + (d == raw.dims - 1 ? "" : ",");
        }
        shape_info += "]";
        if (is_yolox_) {
            for (int i = 0; i < raw.size[1]; ++i) {
                float* row = raw.ptr<float>(0, i);
                max_obj = std::max(max_obj, row[4]);
                max_c0  = std::max(max_c0,  row[5]);
                max_c1  = std::max(max_c1,  row[6]);
                max_c2  = std::max(max_c2,  row[7]);
                max_comb0 = std::max(max_comb0, row[4] * row[5]);
                max_comb1 = std::max(max_comb1, row[4] * row[6]);
                max_comb2 = std::max(max_comb2, row[4] * row[7]);
            }
        }
        std::string class_info = " max_obj=" + std::to_string(max_obj).substr(0, 5)
                               + " max_c0=" + std::to_string(max_c0).substr(0, 5)
                               + " max_c1=" + std::to_string(max_c1).substr(0, 5)
                               + " max_c2=" + std::to_string(max_c2).substr(0, 5)
                               + " comb0=" + std::to_string(max_comb0).substr(0, 5)
                               + " comb1=" + std::to_string(max_comb1).substr(0, 5)
                               + " comb2=" + std::to_string(max_comb2).substr(0, 5);

        std::cout << "[detect] above_thresh=" << scores.size()
                  << " after_nms=" << keep.size()
                  << src_info << shape_info << class_info << "\n";
    }

    std::vector<Detection> result;
    for (int k : keep)
        result.push_back({boxes[k], scores[k]});
    return result;
}

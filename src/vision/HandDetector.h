#pragma once
#include "../Types.h"
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

struct Detection {
    cv::Rect2f bbox;  // normalised [0,1]
    float      conf;
};

class HandDetector {
public:
    explicit HandDetector(const std::string& model_path,
                          float conf_thresh = 0.25f,
                          float iou_thresh  = 0.45f);

    bool load();
    std::vector<Detection> detect(const cv::Mat& confidence_frame,
                                  const cv::Mat& color_frame = cv::Mat{});

private:
    std::string     model_path_;
    float           conf_thresh_;
    float           iou_thresh_;
    bool            loaded_ = false;
    cv::dnn::Net    net_;

    // YOLOv8n hand model: 320x320 input, output [1,5,2100] = [cx,cy,w,h,conf]
    static constexpr int INPUT_SIZE = 320;

    cv::Mat preprocess(const cv::Mat& src, float& scale, int& pad_x, int& pad_y);
};

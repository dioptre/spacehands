#pragma once
#include "../Types.h"
#include "HandDetector.h"
#include "DepthEstimator.h"
#include <opencv2/core.hpp>
#include <vector>

class GestureClassifier {
public:
    // Converts raw detections + depth map into a tracked HandList.
    HandList classify(const std::vector<Detection>& detections,
                      const cv::Mat& depth_frame,
                      const cv::Mat& confidence_frame,
                      int frame_w, int frame_h,
                      const cv::Mat& color_frame = cv::Mat{});

private:
    HandList prev_;

    Gesture inferGesture(const cv::Mat& confidence_frame,
                         const cv::Rect2f& norm_bbox,
                         int frame_w, int frame_h);

    // Simple IOU-based ID assignment across frames
    void assignIds(HandList& current);
};

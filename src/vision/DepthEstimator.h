#pragma once
#include <opencv2/core.hpp>

struct DepthResult {
    float normalized; // 0=close, 1=far
    float mm;         // raw median depth in mm
};

// Estimates depth for a bounding box region from the depth frame.
class DepthEstimator {
public:
    static constexpr float BUCKET_SIZE_MM = 300.f; // mm per depth bucket

    static DepthResult estimate(const cv::Mat& depth_frame,
                                const cv::Rect2f& norm_bbox,
                                float min_mm = 200.f,
                                float max_mm = 2000.f);

    static int bucket(float z_mm) {
        return static_cast<int>(z_mm / BUCKET_SIZE_MM);
    }
};

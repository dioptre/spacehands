#include "DepthEstimator.h"
#include <algorithm>
#include <vector>

DepthResult DepthEstimator::estimate(const cv::Mat& depth_frame,
                                      const cv::Rect2f& norm_bbox,
                                      float min_mm, float max_mm) {
    int W = depth_frame.cols;
    int H = depth_frame.rows;

    // Convert normalised bbox → pixel rect, shrink to centre 50% to avoid edges
    float cx = (norm_bbox.x + norm_bbox.width  * 0.5f) * W;
    float cy = (norm_bbox.y + norm_bbox.height * 0.5f) * H;
    float rw = norm_bbox.width  * W * 0.25f;
    float rh = norm_bbox.height * H * 0.25f;

    cv::Rect roi(
        (int)std::max(cx - rw, 0.f),
        (int)std::max(cy - rh, 0.f),
        (int)std::max(rw * 2, 1.f),
        (int)std::max(rh * 2, 1.f)
    );
    // Clamp to frame
    roi &= cv::Rect(0, 0, W, H);
    if (roi.empty()) return {0.5f, (min_mm + max_mm) * 0.5f};

    cv::Mat region = depth_frame(roi).clone();
    region = region.reshape(1, region.total());

    // Extract and filter out invalid/zero depth values (ToF dropout noise)
    std::vector<float> vals;
    vals.reserve(region.total());
    for (auto it = region.begin<float>(); it != region.end<float>(); ++it) {
        float val = *it;
        if (val > 10.0f && val < max_mm * 1.5f) {
            vals.push_back(val);
        }
    }

    float median;
    if (vals.empty()) {
        median = (min_mm + max_mm) * 0.5f; // default to center of range
    } else {
        std::sort(vals.begin(), vals.end());
        median = vals[vals.size() / 2];
    }

    float norm = 1.f - std::clamp((median - min_mm) / (max_mm - min_mm), 0.f, 1.f);
    return { norm, median };
}

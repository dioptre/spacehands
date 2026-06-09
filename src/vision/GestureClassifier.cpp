#include "GestureClassifier.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <numeric>

static float iouRect(const cv::Rect2f& a, const cv::Rect2f& b) {
    auto inter = (a & b);
    if (inter.area() <= 0) return 0.f;
    return inter.area() / (a.area() + b.area() - inter.area());
}

// Infer gesture from the shape/fill of the hand blob in the confidence image.
// This is a heuristic classifier — good enough for 5 coarse gestures.
// A trained classifier can replace this later.
Gesture GestureClassifier::inferGesture(const cv::Mat& confidence_frame,
                                         const cv::Rect2f& norm_bbox,
                                         int W, int H) {
    cv::Rect roi(
        (int)(norm_bbox.x * W),
        (int)(norm_bbox.y * H),
        (int)(norm_bbox.width  * W),
        (int)(norm_bbox.height * H)
    );
    roi &= cv::Rect(0, 0, W, H);
    if (roi.area() < 4) return Gesture::UNKNOWN;

    cv::Mat region;
    confidence_frame(roi).convertTo(region, CV_8U, 255.f / 1024.f);
    cv::threshold(region, region, 60, 255, cv::THRESH_BINARY);

    // Aspect ratio: tall+narrow → POINT, wide+short → SPREAD, near-square → FIST/OPEN
    float aspect = (float)roi.width / (float)roi.height;
    // Fill ratio: fraction of bbox that is bright (hand pixels)
    double fill = (double)cv::countNonZero(region) / region.total();

    // THUMBS_UP / THUMBS_DOWN: tall narrow blob with mass skewed to one half
    if (aspect < 0.6f && fill < 0.55f) {
        cv::Mat top_half    = region(cv::Rect(0, 0, region.cols, region.rows/2));
        cv::Mat bottom_half = region(cv::Rect(0, region.rows/2, region.cols, region.rows/2));
        double top_fill    = (double)cv::countNonZero(top_half)    / top_half.total();
        double bottom_fill = (double)cv::countNonZero(bottom_half) / bottom_half.total();
        if (top_fill    > bottom_fill * 1.8) return Gesture::THUMBS_UP;
        if (bottom_fill > top_fill    * 1.8) return Gesture::THUMBS_DOWN;
    }
    if (aspect < 0.45f)                return Gesture::POINT;
    if (fill > 0.75f && aspect > 1.3f) return Gesture::SPREAD;
    if (fill < 0.45f)                  return Gesture::FIST;
    if (fill > 0.6f)                   return Gesture::OPEN;
    return Gesture::PINCH;
}

void GestureClassifier::assignIds(HandList& current) {
    // Greedy IOU matching to keep stable IDs across frames
    std::vector<bool> matched(prev_.size(), false);
    int next_new_id = 0;
    for (auto& h : prev_)
        next_new_id = std::max(next_new_id, h.id + 1);

    for (auto& cur : current) {
        float best_iou = 0.05f; // low threshold — prioritise ID stability
        int   best_idx = -1;
        cv::Rect2f cur_rect(cur.x - 0.05f, cur.y - 0.05f, 0.1f, 0.1f);
        for (size_t i = 0; i < prev_.size(); ++i) {
            if (matched[i]) continue;
            cv::Rect2f prev_rect(prev_[i].x - 0.05f, prev_[i].y - 0.05f, 0.1f, 0.1f);
            float iou = iouRect(cur_rect, prev_rect);
            if (iou > best_iou) { best_iou = iou; best_idx = (int)i; }
        }
        if (best_idx >= 0) {
            cur.id = prev_[best_idx].id;
            matched[best_idx] = true;
        } else {
            cur.id = next_new_id++;
        }
    }
}

HandList GestureClassifier::classify(const std::vector<Detection>& detections,
                                      const cv::Mat& depth_frame,
                                      const cv::Mat& confidence_frame,
                                      int W, int H,
                                      const cv::Mat& /*color_frame*/) {
    HandList hands;
    for (const auto& det : detections) {
        Hand h;
        auto depth = DepthEstimator::estimate(depth_frame, det.bbox);
        h.x      = det.bbox.x + det.bbox.width  * 0.5f;
        h.y      = det.bbox.y + det.bbox.height * 0.5f;
        h.bw     = det.bbox.width;
        h.bh     = det.bbox.height;
        h.z      = depth.normalized;
        h.z_mm   = depth.mm;
        h.bucket = DepthEstimator::bucket(depth.mm);
        h.conf   = det.conf;
        h.gesture = inferGesture(confidence_frame, det.bbox, W, H);
        hands.push_back(h);
    }

    // Sort left-to-right for consistent ordering before ID assignment
    std::sort(hands.begin(), hands.end(), [](const Hand& a, const Hand& b){ return a.x < b.x; });
    assignIds(hands);

    // Smooth position and Z velocity from previous frame
    static constexpr float SMOOTH = 0.35f; // lerp speed — lower = smoother
    for (auto& h : hands) {
        auto it = std::find_if(prev_.begin(), prev_.end(),
                               [&](const Hand& p){ return p.id == h.id; });
        if (it != prev_.end()) {
            h.z_vel = h.z_mm - it->z_mm;
            // Smooth x/y to reduce jitter from bbox size changes
            h.x  = it->x  + (h.x  - it->x)  * SMOOTH;
            h.y  = it->y  + (h.y  - it->y)  * SMOOTH;
            h.bw = it->bw + (h.bw - it->bw) * SMOOTH;
            h.bh = it->bh + (h.bh - it->bh) * SMOOTH;
        } else {
            h.z_vel = 0.f;
        }
    }

    prev_ = hands;
    return hands;
}

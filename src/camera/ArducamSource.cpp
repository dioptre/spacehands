#ifndef PLATFORM_MAC
#include "ArducamSource.h"
#include <iostream>
#include <opencv2/core.hpp>

using namespace Arducam;

ArducamSource::ArducamSource(int device, int max_range_mm)
    : device_(device), max_range_(max_range_mm) {}

bool ArducamSource::open() {
    if (tof_.open(Connection::CSI, device_)) {
        std::cerr << "[ArducamSource] failed to open camera\n";
        return false;
    }
    if (tof_.start(FrameType::DEPTH_FRAME)) {
        std::cerr << "[ArducamSource] failed to start camera\n";
        return false;
    }
    tof_.setControl(Control::RANGE, max_range_);
    auto info = tof_.getCameraInfo();
    w_ = info.width;
    h_ = info.height;
    std::cout << "[ArducamSource] opened " << w_ << "x" << h_ << "\n";
    return true;
}

bool ArducamSource::close() {
    tof_.stop();
    tof_.close();
    return true;
}

bool ArducamSource::nextFrame(CameraFrame& out, int timeout_ms) {
    ArducamFrameBuffer* frame = tof_.requestFrame(timeout_ms);
    if (!frame) return false;

    FrameFormat fmt;
    frame->getFormat(FrameType::DEPTH_FRAME, fmt);
    w_ = fmt.width;
    h_ = fmt.height;

    float* depth_ptr      = (float*)frame->getData(FrameType::DEPTH_FRAME);
    float* confidence_ptr = (float*)frame->getData(FrameType::CONFIDENCE_FRAME);

    // Wrap in cv::Mat without copying, then clone so we own the data before release
    cv::Mat depth_view     (h_, w_, CV_32F, depth_ptr);
    cv::Mat confidence_view(h_, w_, CV_32F, confidence_ptr);
    out.depth      = depth_view.clone();
    out.confidence = confidence_view.clone();

    tof_.releaseFrame(frame);
    return true;
}
#endif // PLATFORM_MAC

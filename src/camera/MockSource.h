#pragma once
#include "CameraSource.h"
#include <opencv2/videoio.hpp>
#include <string>

// Mac dev: reads a grayscale video file and converts to the same float32
// confidence+depth format that ArducamSource produces. If no video path is
// given, generates a synthetic sine-wave pattern so the pipeline still runs.
class MockSource : public CameraSource {
public:
    explicit MockSource(const std::string& video_path = "", int w = 240, int h = 180);
    bool open()  override;
    bool close() override;
    bool nextFrame(CameraFrame& out, int timeout_ms = 200) override;
    int  width()  const override { return w_; }
    int  height() const override { return h_; }

private:
    std::string path_;
    int w_, h_;
    cv::VideoCapture cap_;
    int frame_idx_  = 0;
    int fail_count_ = 0;
};

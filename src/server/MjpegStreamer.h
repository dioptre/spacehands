#pragma once
#include <mutex>
#include <opencv2/core.hpp>
#include <string>
#include <thread>
#include <vector>

// Serves an MJPEG stream of annotated IR frames on /stream.mjpeg.
// Chromium binds this as a video texture in the WebGL shader.
class MjpegStreamer {
public:
    explicit MjpegStreamer(int port);
    void start();
    void stop();
    void pushFrame(const cv::Mat& frame); // expects CV_8U or CV_32F confidence frame

private:
    int         port_;
    std::thread thread_;

    std::mutex        mutex_;
    std::vector<uchar> jpeg_buf_;
    bool              has_frame_ = false;
};

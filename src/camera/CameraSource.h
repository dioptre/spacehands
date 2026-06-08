#pragma once
#include <opencv2/core.hpp>

struct CameraFrame {
    cv::Mat confidence; // CV_32F, 0–1024, IR amplitude — used for MJPEG/shader
    cv::Mat depth;      // CV_32F, millimetres
    cv::Mat color;      // CV_8UC3, BGR — used for YOLO detection (colour model)
};

class CameraSource {
public:
    virtual ~CameraSource() = default;
    virtual bool open()  = 0;
    virtual bool close() = 0;
    // Blocks until a frame is ready (or timeout_ms elapses). Returns false on error.
    virtual bool nextFrame(CameraFrame& out, int timeout_ms = 200) = 0;
    virtual int  width()  const = 0;
    virtual int  height() const = 0;
};

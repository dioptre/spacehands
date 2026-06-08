#pragma once
#ifndef PLATFORM_MAC
#include "CameraSource.h"
#include <ArducamTOFCamera.hpp>

class ArducamSource : public CameraSource {
public:
    explicit ArducamSource(int device = 0, int max_range_mm = 4000);
    bool open()  override;
    bool close() override;
    bool nextFrame(CameraFrame& out, int timeout_ms = 200) override;
    int  width()  const override { return w_; }
    int  height() const override { return h_; }

private:
    int device_;
    int max_range_;
    int w_ = 240, h_ = 180;
    Arducam::ArducamTOFCamera tof_;
};
#endif // PLATFORM_MAC

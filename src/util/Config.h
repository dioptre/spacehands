#pragma once
#include <string>

struct Config {
    // camera
    std::string camera      = "mock"; // "arducam" on Pi
    std::string mock_video  = "";     // path to grayscale video, empty = synthetic
    int camera_width        = 240;
    int camera_height       = 180;
    int target_fps          = 30;

    // model
    std::string yolo_model  = "models/yolov8n-hand-int8";
    float yolo_conf         = 0.45f;
    float yolo_iou          = 0.5f;

    // network
    std::string osc_host    = "127.0.0.1";
    int osc_port            = 57120;
    int ws_port             = 8081;
    int http_port           = 8080;
    int mjpeg_port          = 8082;

    static Config load(const std::string& path);
};

#include "Config.h"
#include <fstream>
#include <iostream>
#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
#else
#  include "../../third_party/json.hpp"
#endif

Config Config::load(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[Config] " << path << " not found, using defaults\n";
        return cfg;
    }
    try {
        auto j = nlohmann::json::parse(f);
        if (j.contains("camera"))       cfg.camera      = j["camera"];
        if (j.contains("mock_video"))   cfg.mock_video  = j["mock_video"];
        if (j.contains("camera_width")) cfg.camera_width  = j["camera_width"];
        if (j.contains("camera_height"))cfg.camera_height = j["camera_height"];
        if (j.contains("target_fps"))   cfg.target_fps  = j["target_fps"];
        if (j.contains("yolo_model"))   cfg.yolo_model  = j["yolo_model"];
        if (j.contains("yolo_conf"))    cfg.yolo_conf   = j["yolo_conf"];
        if (j.contains("yolo_iou"))     cfg.yolo_iou    = j["yolo_iou"];
        if (j.contains("mirror_x"))      cfg.mirror_x       = j["mirror_x"];
        if (j.contains("sc_instruments")) cfg.sc_instruments = j["sc_instruments"];
        if (j.contains("coord_x_min"))  cfg.coord_x_min = j["coord_x_min"];
        if (j.contains("coord_x_max"))  cfg.coord_x_max = j["coord_x_max"];
        if (j.contains("coord_y_min"))  cfg.coord_y_min = j["coord_y_min"];
        if (j.contains("coord_y_max"))  cfg.coord_y_max = j["coord_y_max"];
        if (j.contains("coord_z_min"))  cfg.coord_z_min = j["coord_z_min"];
        if (j.contains("coord_z_max"))  cfg.coord_z_max = j["coord_z_max"];
        if (j.contains("hand_x_min"))   cfg.hand_x_min  = j["hand_x_min"];
        if (j.contains("hand_x_max"))   cfg.hand_x_max  = j["hand_x_max"];
        if (j.contains("hand_y_min"))   cfg.hand_y_min  = j["hand_y_min"];
        if (j.contains("hand_y_max"))   cfg.hand_y_max  = j["hand_y_max"];
        if (j.contains("osc_host"))     cfg.osc_host    = j["osc_host"];
        if (j.contains("osc_port"))     cfg.osc_port    = j["osc_port"];
        if (j.contains("ws_port"))      cfg.ws_port     = j["ws_port"];
        if (j.contains("http_port"))    cfg.http_port   = j["http_port"];
        if (j.contains("mjpeg_port"))   cfg.mjpeg_port  = j["mjpeg_port"];
        if (j.contains("enable_visualizer")) cfg.enable_visualizer = j["enable_visualizer"];
        if (j.contains("visualizer_width"))  cfg.visualizer_width  = j["visualizer_width"];
        if (j.contains("visualizer_height")) cfg.visualizer_height = j["visualizer_height"];
        if (j.contains("fullscreen"))        cfg.fullscreen        = j["fullscreen"];
        if (j.contains("crop_left"))         cfg.crop_left         = j["crop_left"];
        if (j.contains("crop_right"))        cfg.crop_right        = j["crop_right"];
    } catch (const std::exception& e) {
        std::cerr << "[Config] parse error: " << e.what() << "\n";
    }
    return cfg;
}

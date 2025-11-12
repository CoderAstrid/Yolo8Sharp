#pragma once
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <tchar.h>
#include "yolo_define.h"
#include "yolo/YOLO11.h"
#include "yolo/YOLO11CLASS.h"
#include "yolo/YOLO11-POSE.h"
#include "yolo/YOLO11-OBB.h"
#include "yolo/YOLO11Seg.h"


/*
struct YoloDet {
    cv::Rect2f box;
    float conf{};
    int cls{};
};

struct YoloPose {
    cv::Rect2f box;
    std::vector<cv::Point2f> kps; // with size N, if pose
    float conf{};
    int cls{};
};

struct YoloSeg {
    cv::Rect2f box;
    cv::Mat mask; // 8U same size as frame or ROI, depending on lib
    float conf{};
    int cls{};
};

struct YoloParams {
    YoloTask task = YoloTask::YT_DETECT;
    int imgsz = 640;
    float conf = 0.25f;
    float iou = 0.50f;
    bool useGPU = true;
    std::string onnx;
    std::string labels; // path to txt; can be empty
};
*/

class YoloRunner {
public:
    YoloRunner() = default;
    ~YoloRunner();

    bool Init(YoloTask task, const TCHAR* appPath);
    void Release();

    std::vector<Detection> runDetect(const cv::Mat& frame);
private:
    YoloTask task_;
    std::unique_ptr<YOLO11Detector> detector_;
    std::unique_ptr<YOLO11Classifier> classifier_;
    std::unique_ptr<YOLO11OBBDetector> obb_;
    std::unique_ptr<YOLO11POSEDetector> pose_;
    std::unique_ptr<YOLOv11SegDetector> seg_;
};

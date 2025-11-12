#include "pch.h"
#include "yolo_runner.h"
#include <fstream>
#include <tchar.h>

// YOLOs-CPP headers (vendor/include in your project)
// Detection / Segmentation / Pose

JString MODEL_FNs[YoloTask::YT_MAX] = {
    _T("yolo11m.onnx"),         // Detect
    _T("yolo11m-cls.onnx"),     // Classify
    _T("yolo11m-seg.onnx"),     // Segment
    _T("yolo11m-pose.onnx"),    // Pose
    _T("yolo11m-obb.onnx")      // Oriented Bounding Box Detect
};

JString CFG_FNs[YoloTask::YT_MAX] = {
	_T("coco.names"),           // Detect
    _T("ImageNet.names"),       // Classify
    _T("coco.names"),           // Segment
    _T("coco.names"),           // Pose
    _T("Dota.names")            // Oriented Bounding Box Detect
};

static std::vector<JString> loadLabels(const JString& path)
{
    std::vector<JString> out;
    if (path.empty()) 
        return out;
#if defined(UNICODE) || defined(_UNICODE)
    std::wifstream f(path);
#else
    std::ifstream f(path);
#endif
    JString s;
    while (std::getline(f, s)) 
        if (!s.empty()) 
            out.push_back(s);
    return out;
}

YoloRunner::~YoloRunner() 
{
    Release();
}

bool YoloRunner::Init(YoloTask task, const TCHAR* appPath) 
{
    task_ = task;
    TCHAR fullPath[MAX_PATH];
    _tcscpy_s(fullPath, appPath);  // Copy base path
    // Make sure there is a trailing backslash
    size_t len = _tcslen(fullPath);
    if (len > 0 && fullPath[len - 1] != _T('\\')) {
        _tcscat_s(fullPath, _T("\\"));
    }
    // Append relative path
    _tcscat_s(fullPath, _T("model\\"));
    _tcscat_s(fullPath, MODEL_FNs[static_cast<int>(task)].c_str());

    TCHAR fullPathcfg[MAX_PATH];
    _tcscpy_s(fullPathcfg, appPath);  // Copy base path
    // Make sure there is a trailing backslash
    len = _tcslen(fullPathcfg);
    if (len > 0 && fullPathcfg[len - 1] != _T('\\')) {
        _tcscat_s(fullPathcfg, _T("\\"));
    }
    // Append relative path
    _tcscat_s(fullPathcfg, _T("cfg\\"));
    _tcscat_s(fullPathcfg, CFG_FNs[static_cast<int>(task)].c_str());

    try {
        switch (task) {
        case YT_DETECT:
            detector_ = std::make_unique<YOLO11Detector>(fullPath, fullPathcfg, true);
            break;
        case YT_CLASSIFY:
            classifier_ = std::make_unique<YOLO11Classifier>(fullPath, fullPathcfg, true);
            break;
        case YT_OBB:
            obb_ = std::make_unique<YOLO11OBBDetector>(fullPath, fullPathcfg, true);
            break;
        case YT_POSE:
            pose_ = std::make_unique<YOLO11POSEDetector>(fullPath, fullPathcfg, true);
            break;
        case YT_SEGMENT:
            seg_ = std::make_unique<YOLOv11SegDetector>(fullPath, fullPathcfg, true);
            break;
        default:
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[YoloRunner] Load failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void YoloRunner::Release() 
{
    detector_.reset();
    classifier_.reset();
    obb_.reset();
    pose_.reset();
    seg_.reset();
    task_ = YT_MAX; // Optional: indicate invalid
}

std::vector<Detection>  YoloRunner::runDetect(const cv::Mat& frame)
{
    if (!detector_)
        return {};
    std::vector<Detection> result = detector_->detect(frame);
    return result;
}
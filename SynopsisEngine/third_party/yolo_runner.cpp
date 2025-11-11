#include "pch.h"
#include "yolo_runner.h"
#include <fstream>
#include <tchar.h>

// YOLOs-CPP headers (vendor/include in your project)
// Detection / Segmentation / Pose

String MODEL_FNs[YoloTask::YT_MAX] = {
    _T("yolo11m.onnx"),         // Detect
    _T("yolo11m-cls.onnx"),     // Classify
    _T("yolo11m-seg.onnx"),     // Segment
    _T("yolo11m-pose.onnx"),    // Pose
    _T("yolo11m-obb.onnx")      // Oriented Bounding Box Detect
};

String CFG_FNs[YoloTask::YT_MAX] = {
	_T("coco.names"),           // Detect
    _T("ImageNet.names"),       // Classify
    _T("coco.names"),           // Segment
    _T("coco.names"),           // Pose
    _T("Dota.names")            // Oriented Bounding Box Detect
};

static std::vector<std::string> loadLabels(const String& path) 
{
    std::vector<std::string> out;
    if (path.empty()) return out;
    std::ifstream f(path);
    std::string s;
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
    try {
        env_ = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "YoloEngine");

        sessionOptions_.SetIntraOpNumThreads(1);
        sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Enable GPU (if desired)
        // OrtCUDAProviderOptions cudaOptions;
        // sessionOptions_.AppendExecutionProvider_CUDA(cudaOptions);

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

        session_ = Ort::Session(env_, fullPath, sessionOptions_);

        // Fetch input/output names for later
        size_t numInputs = session_.GetInputCount();
        for (size_t i = 0; i < numInputs; ++i) {
            inputNames_.emplace_back(session_.GetInputNameAllocated(i, allocator_).get());
        }

        size_t numOutputs = session_.GetOutputCount();
        for (size_t i = 0; i < numOutputs; ++i) {
            outputNames_.emplace_back(session_.GetOutputNameAllocated(i, allocator_).get());
        }

        isInitialized_ = true;
        return true;
    }
    catch (const Ort::Exception& e) {
        OutputDebugStringA(e.what());
        return false;
    }
}

void YoloRunner::Release() {
    session_.release();
    isInitialized_ = false;
}

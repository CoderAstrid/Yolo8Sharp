#pragma once
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

enum class YoloTask { Detect = 0, Segment = 1, Pose = 2 };

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
    YoloTask task = YoloTask::Detect;
    int imgsz = 640;
    float conf = 0.25f;
    float iou = 0.50f;
    bool useGPU = true;
    std::string onnx;
    std::string labels; // path to txt; can be empty
};

class YoloRunner {
public:
    bool Init(const YoloParams& p, std::string* err = nullptr);
    bool Ready() const { return ready_; }
    void Close();

    // Run on a BGR frame; only fill the vector that matches task
    bool Run(const cv::Mat& bgr,
        std::vector<YoloDet>* outDet,
        std::vector<YoloSeg>* outSeg,
        std::vector<YoloPose>* outPose,
        double* ms_pre = nullptr, double* ms_infer = nullptr, double* ms_post = nullptr);

    const std::vector<std::string>& Classes() const { return classes_; }

private:
    bool ready_ = false;
    YoloParams P_;
    std::vector<std::string> classes_;

    // Pointers for exactly one active task
    struct DetectAPI; std::unique_ptr<DetectAPI> detect_;
    struct SegAPI;    std::unique_ptr<SegAPI>    seg_;
    struct PoseAPI;   std::unique_ptr<PoseAPI>   pose_;
};

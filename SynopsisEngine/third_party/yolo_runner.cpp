#include "yolo_runner.h"
#include <fstream>

// YOLOs-CPP headers (vendor/include in your project)
// Detection / Segmentation / Pose
#include "yolo/YOLO11.hpp"
#include "yolo/YOLO11Seg.hpp"
#include "yolo/YOLO11-POSE.hpp"

static std::vector<std::string> loadLabels(const std::string& path) {
    std::vector<std::string> out;
    if (path.empty()) return out;
    std::ifstream f(path);
    std::string s; while (std::getline(f, s)) if (!s.empty()) out.push_back(s);
    return out;
}

struct YoloRunner::DetectAPI {
    std::unique_ptr<YOLO11Detector> det;
    explicit DetectAPI(const std::string& onnx, const std::string& labels, bool gpu) {
        det = std::make_unique<YOLO11Detector>(onnx, labels, gpu);
    }
};
struct YoloRunner::SegAPI {
    std::unique_ptr<YOLOv11SegDetector> seg;
    explicit SegAPI(const std::string& onnx, const std::string& labels, bool gpu) {
        seg = std::make_unique<YOLOv11SegDetector>(onnx, labels, gpu);
    }
};
struct YoloRunner::PoseAPI {
    std::unique_ptr<YOLO11POSEDetector> pose;
    explicit PoseAPI(const std::string& onnx, const std::string& labels, bool gpu) {
        pose = std::make_unique<YOLO11POSEDetector>(onnx, labels, gpu);
    }
};

bool YoloRunner::Init(
    const YoloParams& p, 
    std::string* err
) {
    Close();
    P_ = p;
    classes_ = loadLabels(p.labels);

    try {
        switch (P_.task) {
        case YoloTask::Detect:
            detect_ = std::make_unique<DetectAPI>(P_.onnx, P_.labels, P_.useGPU);
            break;
        case YoloTask::Segment:
            seg_ = std::make_unique<SegAPI>(P_.onnx, P_.labels, P_.useGPU);
            break;
        case YoloTask::Pose:
            pose_ = std::make_unique<PoseAPI>(P_.onnx, P_.labels, P_.useGPU);
            break;
        }
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
    ready_ = true;
    return true;
}

bool YoloRunner::Run(
    const cv::Mat& bgr,
    std::vector<YoloDet>* outDet,
    std::vector<YoloSeg>* outSeg,
    std::vector<YoloPose>* outPose,
    double* /*ms_pre*/, double* /*ms_infer*/, double* /*ms_post*/
) {
    if (!ready_ || bgr.empty()) 
        return false;

    try {
        if (detect_) {
            auto dets = detect_->det->detect(bgr);
            if (outDet) {
                outDet->clear();
                outDet->reserve(dets.size());
                for (auto& d : dets) {
                    cv::Rect2f box (d.box.x, d.box.y, d.box.width, d.box.height);
                    outDet->push_back({ box, d.conf, d.classId });
                }
            }
        }
        else if (seg_) {
            auto segres = seg_->seg->segment(bgr, P_.conf, P_.iou);
            if (outSeg) {
                outSeg->clear();
                outSeg->reserve(segres.size());
                for (auto& s : segres) {
                    cv::Rect2f box (s.box.x, s.box.y, s.box.width, s.box.height);
                    YoloSeg y{ box, s.mask, s.conf, s.classId };
                    outSeg->push_back(std::move(y));
                }
            }
        }
        else if (pose_) {
            auto poses = pose_->pose->detect(bgr);
            if (outPose) {
                outPose->clear();
                outPose->reserve(poses.size());
                for (auto& p : poses) {
                    YoloPose y; 
                    y.box = cv::Rect2f(p.box.x, p.box.y, p.box.width, p.box.height);                     
                    y.conf = p.conf; y.cls = p.classId;
                    y.kps.reserve(p.keypoints.size());
                    for (auto& k : p.keypoints) 
                        y.kps.emplace_back(k.x, k.y);
                    outPose->push_back(std::move(y));
                }
            }
        }
    }
    catch (const std::exception& e) {
        // OutputDebugStringA((std::string("[YOLO run] ") + e.what() + "\n").c_str());
        return false;
    }
    return true;
}

void YoloRunner::Close() 
{
    ready_ = false;
    detect_.reset();
    seg_.reset();
    pose_.reset();
}

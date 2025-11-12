#pragma once

// ====================================================
// Single YOLOv11 Segmentation and Detection Header File
// ====================================================
//
// This header defines the YOLOv11SegDetector class for performing object detection 
// and segmentation using the YOLOv11 model. It includes necessary libraries, 
// utility structures, and helper functions to facilitate model inference 
// and result post-processing.
//
// Author: Abdalrahman M. Amer, www.linkedin.com/in/abdalrahman-m-amer
// Date: 25.01.2025
//
// ====================================================


#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "YOLO-common.h"
#include "tools/Debug.hpp"
#include "tools/ScopedTimer.hpp"

// ============================================================================
// Debug/Timer Utilities (Optional)
// ============================================================================
#ifdef DEBUG
    #define DEBUG_PRINT(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
    #define DEBUG_PRINT(msg) /* no-op */
#endif

// Simple scoped timer (optional)
/*
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string &name_)
        : name(name_), start(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
#ifdef DEBUG
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "[TIMER] " << name << ": " << ms << " ms" << std::endl;
#endif
    }
private:
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};
*/
// ============================================================================
// Constants / Thresholds
// ============================================================================
static const float CONFIDENCE_THRESHOLD_SEG = 0.40f; // Filter boxes below this confidence
static const float IOU_THRESHOLD_SEG        = 0.45f; // NMS IoU threshold
static const float MASK_THRESHOLD_SEG       = 0.40f; // Slightly lower to capture partial objects



// ============================================================================
// Utility Namespace
// ============================================================================
namespace utils {
    /*
    inline std::vector<std::string> getClassNamesSeg(const std::string &path) {
        std::vector<std::string> classNames;
        std::ifstream f(path);
        if (!f) {
            std::cerr << "[ERROR] Could not open class names file: " << path << std::endl;
            return classNames;
        }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            classNames.push_back(line);
        }
        DEBUG_PRINT("Loaded " << classNames.size() << " class names from " << path);
        return classNames;
    }
    */

	/*
    inline size_t vectorProduct(const std::vector<int64_t> &shape) {
        return std::accumulate(shape.begin(), shape.end(), 1ull, std::multiplies<size_t>());
    }
	*/

    
    

} // namespace utils

// ============================================================================
// YOLOv11SegDetector Class
// ============================================================================
class YOLOv11SegDetector {
public:
    YOLOv11SegDetector(const JString&modelPath,
                      const JString&labelsPath,
                      bool useGPU = false);

    // Main API
    std::vector<Segmentation> segment(const cv::Mat &image,
                                      float confThreshold = CONFIDENCE_THRESHOLD_SEG,
                                      float iouThreshold  = IOU_THRESHOLD_SEG);

    // Draw results
    void drawSegmentationsAndBoxes(cv::Mat &image,
                           const std::vector<Segmentation> &results,
                           float maskAlpha = 0.5f) const;

    void drawSegmentations(cv::Mat &image,
                           const std::vector<Segmentation> &results,
                           float maskAlpha = 0.5f) const;
    // Accessors
    const std::vector<JString> &getClassNames()  const { return classNames;  }
    const std::vector<cv::Scalar>  &getClassColors() const { return classColors; }

private:
    Ort::Env           env;
    Ort::SessionOptions sessionOptions;
    Ort::Session       session{nullptr};

    bool     isDynamicInputShape{false};
    cv::Size inputImageShape; 

    std::vector<Ort::AllocatedStringPtr> inputNameAllocs;
    std::vector<const char*>             inputNames;
    std::vector<Ort::AllocatedStringPtr> outputNameAllocs;
    std::vector<const char*>             outputNames;

    size_t numInputNodes  = 0;
    size_t numOutputNodes = 0;

    std::vector<JString> classNames;
    std::vector<cv::Scalar>  classColors;

    // Helpers
    cv::Mat preprocess(const cv::Mat &image,
                       float *&blobPtr,
                       std::vector<int64_t> &inputTensorShape);

    std::vector<Segmentation> postprocess(const cv::Size &origSize,
                                          const cv::Size &letterboxSize,
                                          const std::vector<Ort::Value> &outputs,
                                          float confThreshold,
                                          float iouThreshold);
};


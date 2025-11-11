#pragma once

// ===================================
// Single YOLOv11-OBB Detector Header File
// ===================================
//
// This header defines the YOLO11-OBB-Detector class for performing object detection using the YOLOv11-OBB model.
// It includes necessary libraries, utility structures, and helper functions to facilitate model inference
// and result postprocessing.
//
// Authors: 
// 1- Abdalrahman M. Amer, www.linkedin.com/in/abdalrahman-m-amer
// 2- Mohamed Samir, www.linkedin.com/in/mohamed-samir-7a730b237/
//
// Date: 11.03.2025
// ================================

/**
 * @file YOLO11-OBB-Detector.hpp
 * @brief Header file for the YOLO11OBBDetector class, responsible for object detection
 *        using the YOLOv11 OBB model with optimized performance for minimal latency.
 */

// Include necessary ONNX Runtime and OpenCV headers
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <random>
#include <unordered_map>
#include <thread>
#include <cmath>

#include "YOLO-common.h"

// Include debug and custom ScopedTimer tools for performance measurement
#include "tools/Debug.hpp"
#include "tools/ScopedTimer.hpp"



const float CONFIDENCE_THRESHOLD_OBB = 0.25f;



/**
 * @namespace utils
 * @brief Namespace containing utility functions for the YOLO11OBBDetector.
 */
namespace utils {

    /**
     * @brief A robust implementation of a clamp function.
     *        Restricts a value to lie within a specified range [low, high].
     *
     * @tparam T The type of the value to clamp. Should be an arithmetic type (int, float, etc.).
     * @param value The value to clamp.
     * @param low The lower bound of the range.
     * @param high The upper bound of the range.
     * @return const T& The clamped value, constrained to the range [low, high].
     *
     * @note If low > high, the function swaps the bounds automatically to ensure valid behavior.
     */
	/*
    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, T>::type
    inline clamp(const T &value, const T &low, const T &high)
    {
        // Ensure the range [low, high] is valid; swap if necessary
        T validLow = low < high ? low : high;
        T validHigh = low < high ? high : low;

        // Clamp the value to the range [validLow, validHigh]
        if (value < validLow)
            return validLow;
        if (value > validHigh)
            return validHigh;
        return value;
    }
	*/

    /**
     * @brief Loads class names from a given file path.
     * 
     * @param path Path to the file containing class names.
     * @return std::vector<std::string> Vector of class names.
     */
    /*std::vector<std::string> getClassNames(const std::string& path) {
        std::vector<std::string> classNames;
        std::ifstream infile(path);

        if (infile) {
            std::string line;
            while (getline(infile, line)) {
                // Remove carriage return if present (for Windows compatibility)
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                classNames.emplace_back(line);
            }
        } else {
            std::cerr << "ERROR: Failed to access class name path: " << path << std::endl;
        }

        DEBUG_PRINT("Loaded " << classNames.size() << " class names from " + path);
        return classNames;
    }
    */

    /**
     * @brief Computes the product of elements in a vector.
     * 
     * @param vector Vector of integers.
     * @return size_t Product of all elements.
     */
	/*
    size_t vectorProduct(const std::vector<int64_t> &vector) {
        return std::accumulate(vector.begin(), vector.end(), 1ull, std::multiplies<size_t>());
    }
	*/

    

    /**
    * @brief Draws oriented bounding boxes with rotation and labels on the image based on detections
    * 
    * @param image Image on which to draw.
    * @param detections Vector of detections.
    * @param classNames Vector of class names corresponding to object IDs.
    * @param colors Vector of colors for each class.
    */
   /*inline void drawBoundingBox(cv::Mat& image, const std::vector<ObbDetection>& detections,
    const std::vector<std::string> &classNames, const std::vector<cv::Scalar> &colors) {
        for (const auto& detection : detections) {
        if (detection.conf < CONFIDENCE_THRESHOLD_OBB) continue;
        if (detection.classId < 0 || static_cast<size_t>(detection.classId) >= classNames.size()) continue;

        // Convert angle from radians to degrees for OpenCV
        float angle_deg = detection.box.angle * 180.0f / CV_PI;

        cv::RotatedRect rect(cv::Point2f(detection.box.x, detection.box.y),
            cv::Size2f(detection.box.width, detection.box.height),
            angle_deg);

        // Convert rotated rectangle to polygon points
        cv::Mat points_mat;
        cv::boxPoints(rect, points_mat);
        points_mat.convertTo(points_mat, CV_32SC1);

        // Draw bounding box
        cv::Scalar color = colors[detection.classId % colors.size()];
        cv::polylines(image, points_mat, true, color, 3, cv::LINE_AA);

        // Prepare label
        std::string label = classNames[detection.classId] + ": " + cv::format("%.1f%%", detection.conf * 100);
        int baseline = 0;
        float fontScale = 0.6;
        int thickness = 1;
        cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_DUPLEX, fontScale, thickness, &baseline);

        // Calculate label position using bounding rect of rotated rectangle
        cv::Rect brect = rect.boundingRect();
        int x = brect.x;
        int y = brect.y - labelSize.height - baseline;

        // Adjust label position if it goes off-screen
        if (y < 0) {
           y = brect.y + brect.height;
           if (y + labelSize.height > image.rows) {
                y = image.rows - labelSize.height;
            }
        }

        x = std::max(0, std::min(x, image.cols - labelSize.width));

        // Draw label background (darker version of box color)
        cv::Scalar labelBgColor = color * 0.6;
        cv::rectangle(image, cv::Rect(x, y, labelSize.width, labelSize.height + baseline), 
        labelBgColor, cv::FILLED);

        // Draw label text
        cv::putText(image, label, cv::Point(x, y + labelSize.height), 
        cv::FONT_HERSHEY_DUPLEX, fontScale, cv::Scalar::all(255), 
        thickness, cv::LINE_AA);
        }
    }
    */

    


};

/**
 * @brief YOLO11-OBB-Detector class handles loading the YOLO model, preprocessing images, running inference, and postprocessing results.
 */
class YOLO11OBBDetector {
public:
    /**
     * @brief Constructor to initialize the YOLO detector with model and label paths.
     * 
     * @param modelPath Path to the ONNX model file.
     * @param labelsPath Path to the file containing class labels.
     * @param useGPU Whether to use GPU for inference (default is false).
     */
    YOLO11OBBDetector(const String &modelPath, const String &labelsPath, bool useGPU = false);
    
    /**
     * @brief Runs detection on the provided image.
     * 
     * @param image Input image for detection.
     * @param confThreshold Confidence threshold to filter detections (default is 0.4).
     * @param iouThreshold IoU threshold for Non-Maximum Suppression (default is 0.45).
     * @return std::vector<Detection> Vector of detections.
     */
    std::vector<ObbDetection> detect(const cv::Mat &image, float confThreshold = 0.25f, float iouThreshold = 0.25);
    
    /**
     * @brief Draws bounding boxes on the image based on detections.
     * 
     * @param image Image on which to draw.
     * @param detections Vector of detections.
     */
    void drawBoundingBox(cv::Mat& image, const std::vector<ObbDetection>& detections) const {
        // utils::drawBoundingBox(image, detections, classNames, classColors);
    }    
    

private:
    Ort::Env env{nullptr};                         // ONNX Runtime environment
    Ort::SessionOptions sessionOptions{nullptr};   // Session options for ONNX Runtime
    Ort::Session session{nullptr};                 // ONNX Runtime session for running inference
    bool isDynamicInputShape{};                    // Flag indicating if input shape is dynamic
    cv::Size inputImageShape;                      // Expected input image shape for the model

    // Vectors to hold allocated input and output node names
    std::vector<Ort::AllocatedStringPtr> inputNodeNameAllocatedStrings;
    std::vector<const char *> inputNames;
    std::vector<Ort::AllocatedStringPtr> outputNodeNameAllocatedStrings;
    std::vector<const char *> outputNames;

    size_t numInputNodes, numOutputNodes;          // Number of input and output nodes in the model

    std::vector<String> classNames;            // Vector of class names loaded from file
    std::vector<cv::Scalar> classColors;            // Vector of colors for each class

    /**
     * @brief Preprocesses the input image for model inference.
     * 
     * @param image Input image.
     * @param blob Reference to pointer where preprocessed data will be stored.
     * @param inputTensorShape Reference to vector representing input tensor shape.
     * @return cv::Mat Resized image after preprocessing.
     */
    cv::Mat preprocess(const cv::Mat &image, float *&blob, std::vector<int64_t> &inputTensorShape);
    
/**
 * @brief Postprocesses the model output to extract detections with oriented bounding boxes.
 * 
 * @param originalImageSize Size of the original input image.
 * @param resizedImageShape Size of the image after preprocessing.
 * @param outputTensors Vector of output tensors from the model.
 * @param confThreshold Confidence threshold to filter detections.
 * @param iouThreshold IoU threshold for Non-Maximum Suppression (using ProbIoU for rotated boxes).
 * @return std::vector<Detection> Vector of detections with oriented bounding boxes.
 */
    std::vector<ObbDetection> postprocess(const cv::Size &originalImageSize,
        const cv::Size &resizedImageShape,
        const std::vector<Ort::Value> &outputTensors,
        float confThreshold, float iouThreshold,
        int topk = 500); // Default argument here
};

#pragma once

// ===================================
// Single YOLOv11 Pose Detector Header File
// ===================================
//
// This header defines the YOLO11PoseDetector class for performing human pose estimation using the YOLOv11 model.
// It includes necessary libraries, utility structures, and helper functions to facilitate model inference,
// keypoint detection, and result visualization.
//
// Authors: 
// 1- Abdalrahman M. Amer, www.linkedin.com/in/abdalrahman-m-amer
// 2- Mohamed Samir, www.linkedin.com/in/mohamed-samir-7a730b237/
//
// Date: 16.03.2025
// ================================

/**
 * @file YOLO11-POSE.hpp
 * @brief Header file for the YOLO11PoseDetector class, responsible for human pose estimation
 *        using the YOLOv11 model with optimized performance for real-time applications.
 */


// Include necessary ONNX Runtime and OpenCV headers
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

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

#include "YOLO-common.h"

// Include debug and custom ScopedTimer tools for performance measurement
#include "tools/Debug.hpp"
#include "tools/ScopedTimer.hpp"








/**
 * @namespace utils
 * @brief Namespace containing utility functions for the YOLO11POSE-Detector.
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
        
    
}


/**
 * @brief YOLO11POSEDetector class handles loading the YOLO model, preprocessing images, running inference, and postprocessing results.
 */
class YOLO11POSEDetector {
public:
    /**
     * @brief Constructor to initialize the YOLO detector with model and label paths.
     * 
     * @param modelPath Path to the ONNX model file.
     * @param labelsPath Path to the file containing class labels.
     * @param useGPU Whether to use GPU for inference (default is false).
     */
    YOLO11POSEDetector(const String &modelPath, const String &labelsPath, bool useGPU = false);
    
    /**
     * @brief Runs detection on the provided image.
     * 
     * @param image Input image for detection.
     * @param confThreshold Confidence threshold to filter detections (default is 0.4).
     * @param iouThreshold IoU threshold for Non-Maximum Suppression (default is 0.45).
     * @return std::vector<Detection> Vector of detections.
     */
    std::vector<PoseDetection> detect(const cv::Mat &image, float confThreshold = 0.4f, float iouThreshold = 0.5f);
 
    /**
     * @brief Draws bounding boxes and keypoints (if available) on the provided image.
     * 
     * @param image Input/output image on which detections will be visualized.
     * @param detections Vector of detected objects containing bounding boxes and keypoints.
     */
    void drawBoundingBox(cv::Mat &image, const std::vector<PoseDetection> &detections) const;

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
     * @brief Postprocesses the model output to extract detections.
     * 
     * @param originalImageSize Size of the original input image.
     * @param resizedImageShape Size of the image after preprocessing.
     * @param outputTensors Vector of output tensors from the model.
     * @param confThreshold Confidence threshold to filter detections.
     * @param iouThreshold IoU threshold for Non-Maximum Suppression.
     * @return std::vector<Detection> Vector of detections.
     */
    std::vector<PoseDetection> postprocess(const cv::Size &originalImageSize, const cv::Size &resizedImageShape,
                                      const std::vector<Ort::Value> &outputTensors,
                                      float confThreshold, float iouThreshold);
    
};


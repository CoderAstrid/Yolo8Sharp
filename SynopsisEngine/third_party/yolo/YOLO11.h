#pragma once

// ===================================
// Single YOLOv11 Detector Header File
// ===================================
//
// This header defines the YOLO11Detector class for performing object detection using the YOLOv11 model.
// It includes necessary libraries, utility structures, and helper functions to facilitate model inference
// and result postprocessing.
//
// Author: Abdalrahman M. Amer, www.linkedin.com/in/abdalrahman-m-amer
// Date: 29.09.2024
//
// ================================

/**
 * @file YOLO11Detector.hpp
 * @brief Header file for the YOLO11Detector class, responsible for object detection
 *        using the YOLOv11 model with optimized performance for minimal latency.
 */

// Include necessary ONNX Runtime and OpenCV headers
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>



#include "YOLO-common.h"
// Include debug and custom ScopedTimer tools for performance measurement
#include "tools/Debug.hpp"
#include "tools/ScopedTimer.hpp"

/**
 * @brief Confidence threshold for filtering detections.
 */
const float CONFIDENCE_THRESHOLD_DET = 0.4f;

/**
 * @brief  IoU threshold for filtering detections.
 */
const float IOU_THRESHOLD_DET = 0.45f;


/**
 * @namespace utils
 * @brief Namespace containing utility functions for the YOLO11Detector.
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
     

    

    /**
     * @brief Draws bounding boxes and labels on the image based on detections.
     * 
     * @param image Image on which to draw.
     * @param detections Vector of detections.
     * @param classNames Vector of class names corresponding to object IDs.
     * @param colors Vector of colors for each class.
     */
    /*inline void drawBoundingBox(cv::Mat& image, const std::vector<Detection>& detections,
                                const std::vector<std::string> &classNames, const std::vector<cv::Scalar> &colors) {
        // Iterate through each detection to draw bounding boxes and labels
        for (const auto& detection : detections) {
            // Skip detections below the confidence threshold
            if (detection.conf <= CONFIDENCE_THRESHOLD_DET)
                continue;

            // Ensure the object ID is within valid range
            if (detection.classId < 0 || static_cast<size_t>(detection.classId) >= classNames.size())
                continue;

            // Select color based on object ID for consistent coloring
            const cv::Scalar& color = colors[detection.classId % colors.size()];

            // Draw the bounding box rectangle
            cv::rectangle(image, cv::Point(detection.box.x, detection.box.y),
                          cv::Point(detection.box.x + detection.box.width, detection.box.y + detection.box.height),
                          color, 2, cv::LINE_AA);

            // Prepare label text with class name and confidence percentage
            std::string label = classNames[detection.classId] + ": " + std::to_string(static_cast<int>(detection.conf * 100)) + "%";

            // Define text properties for labels
            int fontFace = cv::FONT_HERSHEY_SIMPLEX;
            double fontScale = std::min(image.rows, image.cols) * 0.0008;
            const int thickness = std::max(1, static_cast<int>(std::min(image.rows, image.cols) * 0.002));
            int baseline = 0;

            // Calculate text size for background rectangles
            cv::Size textSize = cv::getTextSize(label, fontFace, fontScale, thickness, &baseline);

            // Define positions for the label
            int labelY = std::max(detection.box.y, textSize.height + 5);
            cv::Point labelTopLeft(detection.box.x, labelY - textSize.height - 5);
            cv::Point labelBottomRight(detection.box.x + textSize.width + 5, labelY + baseline - 5);

            // Draw background rectangle for label
            cv::rectangle(image, labelTopLeft, labelBottomRight, color, cv::FILLED);

            // Put label text
            cv::putText(image, label, cv::Point(detection.box.x + 2, labelY - 2), fontFace, fontScale, cv::Scalar(255, 255, 255), thickness, cv::LINE_AA);
        }
    }
    */

    /**
     * @brief Draws bounding boxes and semi-transparent masks on the image based on detections.
     * 
     * @param image Image on which to draw.
     * @param detections Vector of detections.
     * @param classNames Vector of class names corresponding to object IDs.
     * @param classColors Vector of colors for each class.
     * @param maskAlpha Alpha value for the mask transparency.
     */
    /*
    inline void drawBoundingBoxMask(cv::Mat &image, const std::vector<Detection> &detections,
                                    const std::vector<std::string> &classNames, const std::vector<cv::Scalar> &classColors,
                                    float maskAlpha = 0.4f) {
        // Validate input image
        if (image.empty()) {
            std::cerr << "ERROR: Empty image provided to drawBoundingBoxMask." << std::endl;
            return;
        }

        const int imgHeight = image.rows;
        const int imgWidth = image.cols;

        // Precompute dynamic font size and thickness based on image dimensions
        const double fontSize = std::min(imgHeight, imgWidth) * 0.0006;
        const int textThickness = std::max(1, static_cast<int>(std::min(imgHeight, imgWidth) * 0.001));

        // Create a mask image for blending (initialized to zero)
        cv::Mat maskImage(image.size(), image.type(), cv::Scalar::all(0));

        // Pre-filter detections to include only those above the confidence threshold and with valid class IDs
        std::vector<const Detection*> filteredDetections;
        for (const auto& detection : detections) {
            if (detection.conf > CONFIDENCE_THRESHOLD_DET &&
                detection.classId >= 0 && 
                static_cast<size_t>(detection.classId) < classNames.size()) {
                filteredDetections.emplace_back(&detection);
            }
        }

        // Draw filled rectangles on the mask image for the semi-transparent overlay
        for (const auto* detection : filteredDetections) {
            cv::Rect box(detection->box.x, detection->box.y, detection->box.width, detection->box.height);
            const cv::Scalar &color = classColors[detection->classId];
            cv::rectangle(maskImage, box, color, cv::FILLED);
        }

        // Blend the maskImage with the original image to apply the semi-transparent masks
        cv::addWeighted(maskImage, maskAlpha, image, 1.0f, 0, image);

        // Draw bounding boxes and labels on the original image
        for (const auto* detection : filteredDetections) {
            cv::Rect box(detection->box.x, detection->box.y, detection->box.width, detection->box.height);
            const cv::Scalar &color = classColors[detection->classId];
            cv::rectangle(image, box, color, 2, cv::LINE_AA);

            std::string label = classNames[detection->classId] + ": " + std::to_string(static_cast<int>(detection->conf * 100)) + "%";
            int baseLine = 0;
            cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontSize, textThickness, &baseLine);

            int labelY = std::max(detection->box.y, labelSize.height + 5);
            cv::Point labelTopLeft(detection->box.x, labelY - labelSize.height - 5);
            cv::Point labelBottomRight(detection->box.x + labelSize.width + 5, labelY + baseLine - 5);

            // Draw background rectangle for label
            cv::rectangle(image, labelTopLeft, labelBottomRight, color, cv::FILLED);

            // Put label text
            cv::putText(image, label, cv::Point(detection->box.x + 2, labelY - 2), cv::FONT_HERSHEY_SIMPLEX, fontSize, cv::Scalar(255, 255, 255), textThickness, cv::LINE_AA);
        }

        DEBUG_PRINT("Bounding boxes and masks drawn on image.");
    }
    */
};

/**
 * @brief 
 * YOLO11Detector class handles loading the YOLO model, 
 * preprocessing images, running inference, and postprocessing results.
 */
class YOLO11Detector {
public:
    /**
     * @brief Constructor to initialize the YOLO detector with model and label paths.
     * 
     * @param modelPath Path to the ONNX model file.
     * @param labelsPath Path to the file containing class labels.
     * @param useGPU Whether to use GPU for inference (default is false).
     */
    YOLO11Detector(const JString&modelPath, const JString&labelsPath, bool useGPU = false);
    
    /**
     * @brief Runs detection on the provided image.
     * 
     * @param image Input image for detection.
     * @param confThreshold Confidence threshold to filter detections (default is 0.4).
     * @param iouThreshold IoU threshold for Non-Maximum Suppression (default is 0.45).
     * @return std::vector<Detection> Vector of detections.
     */
    std::vector<Detection> detect(const cv::Mat &image, float confThreshold = 0.4f, float iouThreshold = 0.45f);
    
    /**
     * @brief Draws bounding boxes on the image based on detections.
     * 
     * @param image Image on which to draw.
     * @param detections Vector of detections.
     */
    void drawBoundingBox(cv::Mat &image, const std::vector<Detection> &detections) const {
        // utils::drawBoundingBox(image, detections, classNames, classColors);
    }
    
    /**
     * @brief Draws bounding boxes and semi-transparent masks on the image based on detections.
     * 
     * @param image Image on which to draw.
     * @param detections Vector of detections.
     * @param maskAlpha Alpha value for mask transparency (default is 0.4).
     */
    void drawBoundingBoxMask(cv::Mat &image, const std::vector<Detection> &detections, float maskAlpha = 0.4f) const {
        // utils::drawBoundingBoxMask(image, detections, classNames, classColors, maskAlpha);
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

    std::vector<JString> classNames;            // Vector of class names loaded from file
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
     * @brief Postprocesses the model output to extract detections.
     * 
     * @param originalImageSize Size of the original input image.
     * @param resizedImageShape Size of the image after preprocessing.
     * @param outputTensors Vector of output tensors from the model.
     * @param confThreshold Confidence threshold to filter detections.
     * @param iouThreshold IoU threshold for Non-Maximum Suppression.
     * @return std::vector<Detection> Vector of detections.
     */
    std::vector<Detection> postprocess(const cv::Size &originalImageSize, const cv::Size &resizedImageShape,
                                      const std::vector<Ort::Value> &outputTensors,
                                      float confThreshold, float iouThreshold);
    
};


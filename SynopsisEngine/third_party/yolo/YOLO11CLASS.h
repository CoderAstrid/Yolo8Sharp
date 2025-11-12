#pragma once

// =====================================
// Single Image Classifier Header File
// =====================================
//
// This header defines the YOLO11Classifier class for performing image classification
// using an ONNX model. It includes necessary libraries, utility structures,
// and helper functions to facilitate model inference and result interpretation.
//
// Author: Abdalrahman M. Amer, www.linkedin.com/in/abdalrahman-m-amer
// Date: 2025-05-15
//
// =====================================

/**
 * @file YOLO11CLASS.hpp
 * @brief Header file for the YOLO11Classifier class, responsible for image classification
 * using an ONNX model with optimized performance for minimal latency.
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
#include <iomanip> // For std::fixed and std::setprecision
#include <sstream> // For std::ostringstream

#include "YOLO-common.h"
// #define DEBUG_MODE // Enable debug mode for detailed logging

// Include debug and custom ScopedTimer tools for performance measurement
// Assuming these are in a common 'tools' directory relative to this header
#include "tools/Debug.hpp"
#include "tools/ScopedTimer.hpp"



/**
 * @namespace utils
 * @brief Namespace containing utility functions for the YOLO11Classifier.
 */
namespace utils {

    // ... (clamp, getClassNames, vectorProduct, preprocessImageToTensor, drawClassificationResult utilities remain the same as previous correct version) ...
    /**
     * @brief A robust implementation of a clamp function.
     * Restricts a value to lie within a specified range [low, high].
     */
    /*template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, T>::type
    inline clamp(const T &value, const T &low, const T &high)
    {
        T validLow = low < high ? low : high;
        T validHigh = low < high ? high : low;

        if (value < validLow)
			return validLow;
        if (value > validHigh) 
			return validHigh;
        return value;
    }
    */

    /**
     * @brief Loads class names from a given file path.
     */
    /*std::vector<std::string> getClassNames(const std::string& path) {
        std::vector<std::string> classNames;
        std::ifstream infile(path);

        if (infile) {
            std::string line;
            while (getline(infile, line)) {
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
     * @brief Draws the classification result on the image.
     */
    /*
    inline void drawClassificationResult(cv::Mat &image, const ClassificationResult &result,
                                         const cv::Point& position = cv::Point(10, 10),
                                         const cv::Scalar& textColor = cv::Scalar(0, 255, 0),
                                         double fontScaleMultiplier = 0.0008,
                                         const cv::Scalar& bgColor = cv::Scalar(0,0,0) ) {
        if (image.empty()) {
            std::cerr << "ERROR: Empty image provided to drawClassificationResult." << std::endl;
            return;
        }
        if (result.classId == -1) {
            DEBUG_PRINT("Skipping drawing due to invalid classification result.");
            return;
        }

        std::ostringstream ss;
        ss << result.className << ": " << std::fixed << std::setprecision(2) << result.confidence * 100 << "%";
        std::string text = ss.str();

        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = std::min(image.rows, image.cols) * fontScaleMultiplier;
        if (fontScale < 0.4) fontScale = 0.4;
        const int thickness = std::max(1, static_cast<int>(fontScale * 1.8));
        int baseline = 0;

        cv::Size textSize = cv::getTextSize(text, fontFace, fontScale, thickness, &baseline);
        baseline += thickness;

        cv::Point textPosition = position;
        if (textPosition.x < 0) textPosition.x = 0;
        if (textPosition.y < textSize.height) textPosition.y = textSize.height + 2; 

        cv::Point backgroundTopLeft(textPosition.x, textPosition.y - textSize.height - baseline / 3);
        cv::Point backgroundBottomRight(textPosition.x + textSize.width, textPosition.y + baseline / 2);
        
        backgroundTopLeft.x = utils::clamp(backgroundTopLeft.x, 0, image.cols -1);
        backgroundTopLeft.y = utils::clamp(backgroundTopLeft.y, 0, image.rows -1);
        backgroundBottomRight.x = utils::clamp(backgroundBottomRight.x, 0, image.cols -1);
        backgroundBottomRight.y = utils::clamp(backgroundBottomRight.y, 0, image.rows -1);

        cv::rectangle(image, backgroundTopLeft, backgroundBottomRight, bgColor, cv::FILLED);
        cv::putText(image, text, cv::Point(textPosition.x, textPosition.y), fontFace, fontScale, textColor, thickness, cv::LINE_AA);

        DEBUG_PRINT("Classification result drawn on image: " << text);
    }
    */
}; // end namespace utils


/**
 * @brief YOLO11Classifier class handles loading the classification model,
 * preprocessing images, running inference, and postprocessing results.
 */
class YOLO11Classifier {
public:
    /**
     * @brief Constructor to initialize the classifier with model and label paths.
     */
    YOLO11Classifier(const JString&modelPath, const JString&labelsPath,
                    bool useGPU = false, const cv::Size& targetInputShape = cv::Size(224, 224));

    /**
     * @brief Runs classification on the provided image.
     */
    ClassificationResult classify(const cv::Mat &image);

    /**
     * @brief Draws the classification result on the image.
     */
    void drawResult(cv::Mat &image, const ClassificationResult &result,
                    const cv::Point& position = cv::Point(10, 10)) const {
        // utils::drawClassificationResult(image, result, position);
    }

    cv::Size getInputShape() const { return inputImageShape_; } // CORRECTED
    bool isModelInputShapeDynamic() const { return isDynamicInputShape_; } // CORRECTED


private:
    Ort::Env env_{nullptr};
    Ort::SessionOptions sessionOptions_{nullptr};
    Ort::Session session_{nullptr};

    bool isDynamicInputShape_{};
    cv::Size inputImageShape_{};

    std::vector<Ort::AllocatedStringPtr> inputNodeNameAllocatedStrings_{};
    std::vector<const char *> inputNames_{};
    std::vector<Ort::AllocatedStringPtr> outputNodeNameAllocatedStrings_{};
    std::vector<const char *> outputNames_{};

    size_t numInputNodes_{}, numOutputNodes_{};
    int numClasses_{0};

    std::vector<JString> classNames_{};

    void preprocess(const cv::Mat &image, float *&blob, std::vector<int64_t> &inputTensorShape);
    ClassificationResult postprocess(const std::vector<Ort::Value> &outputTensors);
};

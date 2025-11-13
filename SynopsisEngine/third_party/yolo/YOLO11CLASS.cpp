#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "YOLO11CLASS.h"
#include "..\..\Logger.h"
#include <tchar.h>
#include <vector>

// Implementation of YOLO11Classifier constructor
YOLO11Classifier::YOLO11Classifier(const JString& modelPath, const JString& labelsPath,
    bool useGPU, const cv::Size& targetInputShape)
    : inputImageShape_(targetInputShape) {
    env_ = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "ONNX_CLASSIFICATION_ENV");
    sessionOptions_ = Ort::SessionOptions();

    sessionOptions_.SetIntraOpNumThreads(std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
    sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    std::vector<std::string> availableProviders = Ort::GetAvailableProviders();
    auto cudaAvailable = std::find(availableProviders.begin(), availableProviders.end(), "CUDAExecutionProvider");
    OrtCUDAProviderOptions cudaOption{};

    if (useGPU && cudaAvailable != availableProviders.end()) {
        LOG_DEBUG("[YOLO11Classifier] Attempting to use GPU for inference.");
        sessionOptions_.AppendExecutionProvider_CUDA(cudaOption);
    }
    else {
        if (useGPU) {
            LOG_WARNING("[YOLO11Classifier] GPU requested but CUDAExecutionProvider is not available. Falling back to CPU.");
        }
        LOG_DEBUG("[YOLO11Classifier] Using CPU for inference.");
    }

    session_ = Ort::Session(env_, modelPath.c_str(), sessionOptions_);

    Ort::AllocatorWithDefaultOptions allocator;

    numInputNodes_ = session_.GetInputCount();
    numOutputNodes_ = session_.GetOutputCount();

    if (numInputNodes_ == 0)
        throw std::runtime_error("Model has no input nodes.");
    if (numOutputNodes_ == 0)
        throw std::runtime_error("Model has no output nodes.");

    auto input_node_name = session_.GetInputNameAllocated(0, allocator);
    inputNodeNameAllocatedStrings_.push_back(std::move(input_node_name));
    inputNames_.push_back(inputNodeNameAllocatedStrings_.back().get());

    Ort::TypeInfo inputTypeInfo = session_.GetInputTypeInfo(0);
    auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> modelInputTensorShapeVec = inputTensorInfo.GetShape();

    if (modelInputTensorShapeVec.size() == 4) {
        isDynamicInputShape_ = (modelInputTensorShapeVec[2] == -1 || modelInputTensorShapeVec[3] == -1);
        LOG_DEBUG_STREAM("[YOLO11Classifier] Model input tensor shape from metadata: "
            << modelInputTensorShapeVec[0] << "x" << modelInputTensorShapeVec[1] << "x"
            << modelInputTensorShapeVec[2] << "x" << modelInputTensorShapeVec[3]);

        if (!isDynamicInputShape_) {
            int modelH = static_cast<int>(modelInputTensorShapeVec[2]);
            int modelW = static_cast<int>(modelInputTensorShapeVec[3]);
            if (modelH != inputImageShape_.height || modelW != inputImageShape_.width) {
                LOG_WARNING_STREAM("[YOLO11Classifier] Target preprocessing shape (" << inputImageShape_.height << "x" << inputImageShape_.width
                    << ") differs from model's fixed input shape (" << modelH << "x" << modelW << "). "
                    << "Image will be preprocessed to " << inputImageShape_.height << "x" << inputImageShape_.width << "."
                    << " Consider aligning these for optimal performance/accuracy.");
            }
        }
        else {
            LOG_DEBUG_STREAM("[YOLO11Classifier] Model has dynamic input H/W. Preprocessing to specified target: "
                << inputImageShape_.height << "x" << inputImageShape_.width);
        }
    }
    else {
        std::ostringstream oss;
        oss << "[YOLO11Classifier] Warning: Model input tensor does not have 4 dimensions as expected (NCHW). Shape: [";
        for (size_t i = 0; i < modelInputTensorShapeVec.size(); ++i) oss << modelInputTensorShapeVec[i] << (i == modelInputTensorShapeVec.size() - 1 ? "" : ", ");
        oss << "]. Assuming dynamic shape and proceeding with target HxW: "
            << inputImageShape_.height << "x" << inputImageShape_.width;
        LOG_WARNING(oss.str());
        isDynamicInputShape_ = true;
    }

    auto output_node_name = session_.GetOutputNameAllocated(0, allocator);
    outputNodeNameAllocatedStrings_.push_back(std::move(output_node_name));
    outputNames_.push_back(outputNodeNameAllocatedStrings_.back().get());

    Ort::TypeInfo outputTypeInfo = session_.GetOutputTypeInfo(0);
    auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outputTensorShapeVec = outputTensorInfo.GetShape();

    if (!outputTensorShapeVec.empty()) {
        if (outputTensorShapeVec.size() == 2 && outputTensorShapeVec[0] > 0) {
            numClasses_ = static_cast<int>(outputTensorShapeVec[1]);
        }
        else if (outputTensorShapeVec.size() == 1 && outputTensorShapeVec[0] > 0) {
            numClasses_ = static_cast<int>(outputTensorShapeVec[0]);
        }
        else {
            for (long long dim : outputTensorShapeVec) {
                if (dim > 1 && numClasses_ == 0) numClasses_ = static_cast<int>(dim);
            }
            if (numClasses_ == 0 && !outputTensorShapeVec.empty()) numClasses_ = static_cast<int>(outputTensorShapeVec.back());
        }
    }

    if (numClasses_ > 0) {
        // CORRECTED SECTION for printing outputTensorShapeVec
        std::ostringstream oss_shape;
        oss_shape << "[";
        for (size_t i = 0; i < outputTensorShapeVec.size(); ++i) {
            oss_shape << outputTensorShapeVec[i];
            if (i < outputTensorShapeVec.size() - 1) {
                oss_shape << ", ";
            }
        }
        oss_shape << "]";
        LOG_DEBUG_STREAM("[YOLO11Classifier] Model predicts " << numClasses_ << " classes based on output shape: " << oss_shape.str());
        // END CORRECTED SECTION
    }
    else {
        std::ostringstream oss;
        oss << "[YOLO11Classifier] Warning: Could not reliably determine number of classes from output shape: [";
        for (size_t i = 0; i < outputTensorShapeVec.size(); ++i) {
            oss << outputTensorShapeVec[i] << (i == outputTensorShapeVec.size() - 1 ? "" : ", ");
        }
        oss << "]. Postprocessing might be incorrect or assume a default.";
        LOG_WARNING(oss.str());
    }

    classNames_ = utils::getClassNames(labelsPath);
    if (numClasses_ > 0 && !classNames_.empty() && classNames_.size() != static_cast<size_t>(numClasses_)) {
        std::wcerr << "Warning: Number of classes from model (" << numClasses_
            << ") does not match number of labels in " << labelsPath
            << " (" << classNames_.size() << ")." << std::endl;
    }
    if (classNames_.empty() && numClasses_ > 0) {
        LOG_WARNING("[YOLO11Classifier] Class names file is empty or failed to load. Predictions will use numeric IDs if labels are not available.");
    }

    // Note: modelPath is JString (wide string), convert for logging
    std::string modelPathStr;
#ifdef UNICODE
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, modelPath.c_str(), -1, NULL, 0, NULL, NULL);
    if (size_needed > 0) {
        std::vector<char> buffer(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, modelPath.c_str(), -1, buffer.data(), size_needed, NULL, NULL);
        modelPathStr = std::string(buffer.data());
    }
#else
    modelPathStr = std::string(modelPath.c_str());
#endif
    LOG_INFO_STREAM("[YOLO11Classifier] YOLO11Classifier initialized successfully. Model: " << modelPathStr);
}

// ... (preprocess, postprocess, and classify methods remain the same as previous correct version) ...
void YOLO11Classifier::preprocess(const cv::Mat& image, float*& blob, std::vector<int64_t>& inputTensorShape) {
    ScopedTimer timer("Preprocessing (Ultralytics-style)");

    if (image.empty()) {
        throw std::runtime_error("Input image to preprocess is empty.");
    }

    cv::Mat processedImage;
    // 1. Resize to target input shape (e.g., 224x224).
    //    The color cv::Scalar(0,0,0) is for padding if 'letterbox' strategy were used; unused for 'resize'.
    utils::preprocessImageToTensor(image, processedImage, inputImageShape_, cv::Scalar(0, 0, 0), true, "resize");

    // 2. Convert BGR (OpenCV default) to RGB
    cv::Mat rgbImageMat; // Use a different name to avoid confusion if processedImage was already RGB
    cv::cvtColor(processedImage, rgbImageMat, cv::COLOR_BGR2RGB);

    // 3. Convert to float32. At this point, values are typically in [0, 255] range.
    cv::Mat floatRgbImage;
    rgbImageMat.convertTo(floatRgbImage, CV_32F);

    // Set the actual NCHW tensor shape for this specific input
    // Model expects NCHW: Batch=1, Channels=3 (RGB), Height, Width
    inputTensorShape = { 1, 3, static_cast<int64_t>(floatRgbImage.rows), static_cast<int64_t>(floatRgbImage.cols) };

    if (static_cast<int>(inputTensorShape[2]) != inputImageShape_.height || static_cast<int>(inputTensorShape[3]) != inputImageShape_.width) {
        LOG_ERROR_STREAM("[YOLO11Classifier] CRITICAL WARNING: Preprocessed image dimensions (" << inputTensorShape[2] << "x" << inputTensorShape[3]
            << ") do not match target inputImageShape_ (" << inputImageShape_.height << "x" << inputImageShape_.width
            << ") after resizing! This indicates an issue in utils::preprocessImageToTensor or logic.");
    }

    size_t tensorSize = utils::vectorProduct(inputTensorShape); // 1 * C * H * W
    blob = new float[tensorSize];

    // 4. Scale pixel values to [0.0, 1.0] and convert HWC to CHW
    int h = static_cast<int>(inputTensorShape[2]); // Height
    int w = static_cast<int>(inputTensorShape[3]); // Width
    int num_channels = static_cast<int>(inputTensorShape[1]); // Should be 3

    if (num_channels != 3) {
        delete[] blob; // Clean up allocated memory
        throw std::runtime_error("Expected 3 channels for image blob after RGB conversion, but tensor shape indicates: " + std::to_string(num_channels));
    }
    if (floatRgbImage.channels() != 3) {
        delete[] blob;
        throw std::runtime_error("Expected 3 channels in cv::Mat floatRgbImage, but got: " + std::to_string(floatRgbImage.channels()));
    }

    for (int c_idx = 0; c_idx < num_channels; ++c_idx) {      // Iterate over R, G, B channels
        for (int i = 0; i < h; ++i) {     // Iterate over rows (height)
            for (int j = 0; j < w; ++j) { // Iterate over columns (width)
                // floatRgbImage is HWC (i is row, j is col, c_idx is channel index in Vec3f)
                // floatRgbImage pixel values are float and in [0, 255] range.
                float pixel_value = floatRgbImage.at<cv::Vec3f>(i, j)[c_idx];

                // Scale to [0.0, 1.0]
                float scaled_pixel = pixel_value / 255.0f;

                // Store in blob (CHW format)
                blob[c_idx * (h * w) + i * w + j] = scaled_pixel;
            }
        }
    }

    LOG_DEBUG_STREAM("[YOLO11Classifier] Preprocessing completed (RGB, scaled [0,1]). Actual input tensor shape: "
        << inputTensorShape[0] << "x" << inputTensorShape[1] << "x"
        << inputTensorShape[2] << "x" << inputTensorShape[3]);
}
ClassificationResult YOLO11Classifier::postprocess(const std::vector<Ort::Value>& outputTensors) {
    ScopedTimer timer("Postprocessing");

    if (outputTensors.empty()) {
        LOG_ERROR("[YOLO11Classifier] No output tensors for postprocessing.");
        return {};
    }

    const float* rawOutput = outputTensors[0].GetTensorData<float>();
    if (!rawOutput) {
        LOG_ERROR("[YOLO11Classifier] rawOutput pointer is null.");
        return {};
    }

    const std::vector<int64_t> outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t numScores = utils::vectorProduct(outputShape);

    // Debug output shape
    std::ostringstream oss_shape;
    oss_shape << "Output tensor shape: [";
    for (size_t i = 0; i < outputShape.size(); ++i) {
        oss_shape << outputShape[i] << (i == outputShape.size() - 1 ? "" : ", ");
    }
    oss_shape << "]";
    LOG_DEBUG_STREAM("[YOLO11Classifier] " << oss_shape.str());

    // Determine the effective number of classes
    int currentNumClasses = numClasses_ > 0 ? numClasses_ : static_cast<int>(classNames_.size());
    if (currentNumClasses <= 0) {
        LOG_ERROR("[YOLO11Classifier] No valid number of classes determined.");
        return {};
    }

    // Debug first few raw scores
    std::ostringstream oss_scores;
    oss_scores << "First few raw scores: ";
    for (size_t i = 0; i < std::min(size_t(5), numScores); ++i) {
        oss_scores << rawOutput[i] << " ";
    }
    LOG_DEBUG_STREAM("[YOLO11Classifier] " << oss_scores.str());

    // Find maximum score and its corresponding class
    int bestClassId = -1;
    float maxScore = -std::numeric_limits<float>::infinity();
    std::vector<float> scores(currentNumClasses);

    // Handle different output shapes
    if (outputShape.size() == 2 && outputShape[0] == 1) {
        // Case 1: [1, num_classes] shape
        for (int i = 0; i < currentNumClasses && i < static_cast<int>(outputShape[1]); ++i) {
            scores[i] = rawOutput[i];
            if (scores[i] > maxScore) {
                maxScore = scores[i];
                bestClassId = i;
            }
        }
    }
    else if (outputShape.size() == 1 || (outputShape.size() == 2 && outputShape[0] > 1)) {
        // Case 2: [num_classes] shape or [batch_size, num_classes] shape (take first batch)
        for (int i = 0; i < currentNumClasses && i < static_cast<int>(numScores); ++i) {
            scores[i] = rawOutput[i];
            if (scores[i] > maxScore) {
                maxScore = scores[i];
                bestClassId = i;
            }
        }
    }

    if (bestClassId == -1) {
        LOG_ERROR("[YOLO11Classifier] Could not determine best class ID.");
        return {};
    }

    // Apply softmax to get probabilities
    float sumExp = 0.0f;
    std::vector<float> probabilities(currentNumClasses);

    // Compute softmax with numerical stability
    for (int i = 0; i < currentNumClasses; ++i) {
        probabilities[i] = std::exp(scores[i] - maxScore);
        sumExp += probabilities[i];
    }

    // Calculate final confidence
    float confidence = sumExp > 0 ? probabilities[bestClassId] / sumExp : 0.0f;

    // Get class name
    JString className = _T("Unknown");
    if (bestClassId >= 0 && static_cast<size_t>(bestClassId) < classNames_.size()) {
        className = classNames_[bestClassId];
    }
    else if (bestClassId >= 0) {
#if defined(UNICODE)
        className = _T("ClassID_") + std::to_wstring(bestClassId);
#else
        className = "ClassID_" + std::to_string(bestClassId);
#endif
    }

    LOG_DEBUG_STREAM("[YOLO11Classifier] Best class ID: " << bestClassId << ", Name: " << className.c_str() << ", Confidence: " << confidence);
    return ClassificationResult(bestClassId, confidence, className);
}

ClassificationResult YOLO11Classifier::classify(const cv::Mat& image) {
    ScopedTimer timer("Overall classification task");

    if (image.empty()) {
        LOG_ERROR("[YOLO11Classifier] Input image for classification is empty.");
        return {};
    }

    float* blobPtr = nullptr;
    std::vector<int64_t> currentInputTensorShape;

    try {
        preprocess(image, blobPtr, currentInputTensorShape);
    }
    catch (const std::exception& e) {
        LOG_ERROR_STREAM("[YOLO11Classifier] Exception during preprocessing: " << e.what());
        if (blobPtr) delete[] blobPtr;
        return {};
    }

    if (!blobPtr) {
        LOG_ERROR("[YOLO11Classifier] Preprocessing failed to produce a valid data blob.");
        return {};
    }

    size_t inputTensorSize = utils::vectorProduct(currentInputTensorShape);
    if (inputTensorSize == 0) {
        LOG_ERROR("[YOLO11Classifier] Input tensor size is zero after preprocessing.");
        delete[] blobPtr;
        return {};
    }

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        blobPtr,
        inputTensorSize,
        currentInputTensorShape.data(),
        currentInputTensorShape.size()
    );

    delete[] blobPtr;
    blobPtr = nullptr;

    std::vector<Ort::Value> outputTensors;
    try {
        outputTensors = session_.Run(
            Ort::RunOptions{ nullptr },
            inputNames_.data(),
            &inputTensor,
            numInputNodes_,
            outputNames_.data(),
            numOutputNodes_
        );
    }
    catch (const Ort::Exception& e) {
        LOG_ERROR_STREAM("[YOLO11Classifier] ONNX Runtime Exception during Run(): " << e.what());
        return {};
    }

    if (outputTensors.empty()) {
        LOG_ERROR("[YOLO11Classifier] ONNX Runtime Run() produced no output tensors.");
        return {};
    }

    try {
        return postprocess(outputTensors);
    }
    catch (const std::exception& e) {
        LOG_ERROR_STREAM("[YOLO11Classifier] Exception during postprocessing: " << e.what());
        return {};
    }
}
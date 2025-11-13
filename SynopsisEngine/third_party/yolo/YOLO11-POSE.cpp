#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "YOLO11-POSE.h"
#include "..\..\Logger.h"


// Implementation of YOLO11POSEDetector constructor
YOLO11POSEDetector::YOLO11POSEDetector(const JString& modelPath, const JString& labelsPath, bool useGPU) {
    // Initialize ONNX Runtime environment with warning level
    env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "ONNX_DETECTION");
    sessionOptions = Ort::SessionOptions();

    // Set number of intra-op threads for parallelism
    sessionOptions.SetIntraOpNumThreads(std::min(6, static_cast<int>(std::thread::hardware_concurrency())));
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Retrieve available execution providers (e.g., CPU, CUDA)
    std::vector<std::string> availableProviders = Ort::GetAvailableProviders();
    auto cudaAvailable = std::find(availableProviders.begin(), availableProviders.end(), "CUDAExecutionProvider");
    OrtCUDAProviderOptions cudaOption;

    // Configure session options based on whether GPU is to be used and available
    if (useGPU && cudaAvailable != availableProviders.end()) {
        LOG_INFO("[YOLO11POSEDetector] Inference device: GPU");
        sessionOptions.AppendExecutionProvider_CUDA(cudaOption); // Append CUDA execution provider
    }
    else {
        if (useGPU) {
            LOG_INFO("[YOLO11POSEDetector] GPU is not supported by your ONNXRuntime build. Fallback to CPU.");
        }
        LOG_INFO("[YOLO11POSEDetector] Inference device: CPU");
    }

    // Load the ONNX model into the session
    session = Ort::Session(env, modelPath.c_str(), sessionOptions);

    Ort::AllocatorWithDefaultOptions allocator;

    // Retrieve input tensor shape information
    Ort::TypeInfo inputTypeInfo = session.GetInputTypeInfo(0);
    std::vector<int64_t> inputTensorShapeVec = inputTypeInfo.GetTensorTypeAndShapeInfo().GetShape();
    isDynamicInputShape = (inputTensorShapeVec.size() >= 4) && (inputTensorShapeVec[2] == -1 && inputTensorShapeVec[3] == -1); // Check for dynamic dimensions

    // Allocate and store input node names
    auto input_name = session.GetInputNameAllocated(0, allocator);
    inputNodeNameAllocatedStrings.push_back(std::move(input_name));
    inputNames.push_back(inputNodeNameAllocatedStrings.back().get());

    // Allocate and store output node names
    auto output_name = session.GetOutputNameAllocated(0, allocator);
    outputNodeNameAllocatedStrings.push_back(std::move(output_name));
    outputNames.push_back(outputNodeNameAllocatedStrings.back().get());

    // Set the expected input image shape based on the model's input tensor
    if (inputTensorShapeVec.size() >= 4) {
        inputImageShape = cv::Size(static_cast<int>(inputTensorShapeVec[3]), static_cast<int>(inputTensorShapeVec[2]));
    }
    else {
        throw std::runtime_error("Invalid input tensor shape.");
    }

    // Get the number of input and output nodes
    numInputNodes = session.GetInputCount();
    numOutputNodes = session.GetOutputCount();


    LOG_INFO_STREAM("[YOLO11POSEDetector] Model loaded successfully with " << numInputNodes << " input nodes and " << numOutputNodes << " output nodes.");
}

// Preprocess function implementation
cv::Mat YOLO11POSEDetector::preprocess(const cv::Mat& image, float*& blob, std::vector<int64_t>& inputTensorShape) {
    ScopedTimer timer("preprocessing");

    cv::Mat resizedImage;
    // Resize and pad the image using letterBox utility
    utils::letterBoxPos(image, resizedImage, inputImageShape, cv::Scalar(114, 114, 114), isDynamicInputShape, false, true, 32);

    // Update input tensor shape based on resized image dimensions
    inputTensorShape[2] = resizedImage.rows;
    inputTensorShape[3] = resizedImage.cols;

    // Convert image to float and normalize to [0, 1]
    resizedImage.convertTo(resizedImage, CV_32FC3, 1 / 255.0f);

    // Allocate memory for the image blob in CHW format
    blob = new float[resizedImage.cols * resizedImage.rows * resizedImage.channels()];

    // Split the image into separate channels and store in the blob
    std::vector<cv::Mat> chw(resizedImage.channels());
    for (int i = 0; i < resizedImage.channels(); ++i) {
        chw[i] = cv::Mat(resizedImage.rows, resizedImage.cols, CV_32FC1, blob + i * resizedImage.cols * resizedImage.rows);
    }
    cv::split(resizedImage, chw); // Split channels into the blob

    LOG_DEBUG("[YOLO11POSEDetector] Preprocessing completed");

    return resizedImage;
}


/**
 * @brief Draws bounding boxes and pose keypoints (if available) on the image.
 *
 * @param image Input/output image where detections will be drawn.
 * @param detections Vector of detected objects, including bounding boxes and keypoints.
 *
 * @note Uses `utils::drawPoseEstimation()` for visualization.
 */
void YOLO11POSEDetector::drawBoundingBox(cv::Mat& image, const std::vector<PoseDetection>& detections) const {
    // utils::drawPoseEstimation(image, detections);
}


/**
 * @brief Processes the raw output tensors from the YOLO model and extracts detections.
 *
 * @param originalImageSize The original size of the input image before resizing.
 * @param resizedImageShape The resized image dimensions used during inference.
 * @param outputTensors The output tensors obtained from the model inference.
 * @param confThreshold Confidence threshold to filter weak detections (default is 0.4).
 * @param iouThreshold IoU threshold for Non-Maximum Suppression (NMS) to remove redundant detections (default is 0.5).
 * @return std::vector<Detection> A vector of final detections after processing.
 *
 * @note This function applies confidence filtering, NMS, and necessary transformations
 *       to map detections back to the original image size.
 */
std::vector<PoseDetection> YOLO11POSEDetector::postprocess(
    const cv::Size& originalImageSize,
    const cv::Size& resizedImageShape,
    const std::vector<Ort::Value>& outputTensors,
    float confThreshold,
    float iouThreshold
) {
    ScopedTimer timer("postprocessing");
    std::vector<PoseDetection> detections;

    const float* rawOutput = outputTensors[0].GetTensorData<float>();
    const std::vector<int64_t> outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();

    // Validate output dimensions
    const size_t numFeatures = outputShape[1];
    const size_t numDetections = outputShape[2];
    const int numKeypoints = 17;
    const int featuresPerKeypoint = 3;

    if (numFeatures != 4 + 1 + numKeypoints * featuresPerKeypoint) {
        LOG_ERROR("[YOLO11POSEDetector] Invalid output shape for pose estimation model");
        return detections;
    }

    // Calculate letterbox padding parameters
    const float scale = std::min(static_cast<float>(resizedImageShape.width) / originalImageSize.width,
        static_cast<float>(resizedImageShape.height) / originalImageSize.height);
    const cv::Size scaledSize(originalImageSize.width * scale, originalImageSize.height * scale);
    const cv::Point2f padding((resizedImageShape.width - scaledSize.width) / 2.0f,
        (resizedImageShape.height - scaledSize.height) / 2.0f);

    // Process each detection
    std::vector<BoundingBox> boxes;
    std::vector<float> confidences;
    std::vector<std::vector<KeyPoint>> allKeypoints;

    for (size_t d = 0; d < numDetections; ++d) {
        const float objConfidence = rawOutput[4 * numDetections + d];
        if (objConfidence < confThreshold) continue;

        // Decode bounding box
        const float cx = rawOutput[0 * numDetections + d];
        const float cy = rawOutput[1 * numDetections + d];
        const float w = rawOutput[2 * numDetections + d];
        const float h = rawOutput[3 * numDetections + d];

        // Convert to original image coordinates
        BoundingBox box;
        box.x = static_cast<int>((cx - padding.x - w / 2) / scale);
        box.y = static_cast<int>((cy - padding.y - h / 2) / scale);
        box.width = static_cast<int>(w / scale);
        box.height = static_cast<int>(h / scale);

        // Clip to image boundaries
        box.x = utils::clamp(box.x, 0, originalImageSize.width - box.width);
        box.y = utils::clamp(box.y, 0, originalImageSize.height - box.height);
        box.width = utils::clamp(box.width, 0, originalImageSize.width - box.x);
        box.height = utils::clamp(box.height, 0, originalImageSize.height - box.y);

        // Extract keypoints
        std::vector<KeyPoint> keypoints;
        for (int k = 0; k < numKeypoints; ++k) {
            const int offset = 5 + k * featuresPerKeypoint;
            KeyPoint kpt;
            kpt.x = (rawOutput[offset * numDetections + d] - padding.x) / scale;
            kpt.y = (rawOutput[(offset + 1) * numDetections + d] - padding.y) / scale;
            kpt.confidence = 1.0f / (1.0f + std::exp(-rawOutput[(offset + 2) * numDetections + d]));

            // Clip keypoints to image boundaries
            kpt.x = utils::clamp(kpt.x, 0.0f, static_cast<float>(originalImageSize.width - 1));
            kpt.y = utils::clamp(kpt.y, 0.0f, static_cast<float>(originalImageSize.height - 1));

            keypoints.emplace_back(kpt);
        }

        // Store detection components
        boxes.emplace_back(box);
        confidences.emplace_back(objConfidence);
        allKeypoints.emplace_back(keypoints);
    }

    // Apply Non-Maximum Suppression
    std::vector<int> indices;
    utils::NMSBoxesPose(boxes, confidences, confThreshold, iouThreshold, indices);

    // Create final detections
    for (int idx : indices) {
        PoseDetection det;
        // After (convert cv::Rect to BoundingBox)
        det.box.x = boxes[idx].x;
        det.box.y = boxes[idx].y;
        det.box.width = boxes[idx].width;
        det.box.height = boxes[idx].height;
        det.conf = confidences[idx];
        det.classId = 0; // Single class (person)
        det.keypoints = allKeypoints[idx];
        detections.emplace_back(det);
    }

    return detections;
}

// Detect function implementation
std::vector<PoseDetection> YOLO11POSEDetector::detect(const cv::Mat& image, float confThreshold, float iouThreshold) {
    ScopedTimer timer("Overall detection");

    float* blobPtr = nullptr; // Pointer to hold preprocessed image data
    // Define the shape of the input tensor (batch size, channels, height, width)
    std::vector<int64_t> inputTensorShape = { 1, 3, inputImageShape.height, inputImageShape.width };

    // Preprocess the image and obtain a pointer to the blob
    cv::Mat preprocessedImage = preprocess(image, blobPtr, inputTensorShape);

    // Compute the total number of elements in the input tensor
    size_t inputTensorSize = utils::vectorProduct(inputTensorShape);

    // Create a vector from the blob data for ONNX Runtime input
    std::vector<float> inputTensorValues(blobPtr, blobPtr + inputTensorSize);

    delete[] blobPtr; // Free the allocated memory for the blob

    // Create an Ort memory info object (can be cached if used repeatedly)
    static Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // Create input tensor object using the preprocessed data
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        inputTensorValues.data(),
        inputTensorSize,
        inputTensorShape.data(),
        inputTensorShape.size()
    );

    // Run the inference session with the input tensor and retrieve output tensors
    std::vector<Ort::Value> outputTensors = session.Run(
        Ort::RunOptions{ nullptr },
        inputNames.data(),
        &inputTensor,
        numInputNodes,
        outputNames.data(),
        numOutputNodes
    );

    // Determine the resized image shape based on input tensor shape
    cv::Size resizedImageShape(static_cast<int>(inputTensorShape[3]), static_cast<int>(inputTensorShape[2]));

    // Postprocess the output tensors to obtain detections
    std::vector<PoseDetection> detections = postprocess(image.size(), resizedImageShape, outputTensors, confThreshold, iouThreshold);

    return detections; // Return the vector of detections
}
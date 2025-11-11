#include "YOLO11-OBB.h"


// Implementation of YOLO11OBBDetector constructor
YOLO11OBBDetector::YOLO11OBBDetector(const String& modelPath, const String& labelsPath, bool useGPU) {
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
        std::cout << "Inference device: GPU" << std::endl;
        sessionOptions.AppendExecutionProvider_CUDA(cudaOption); // Append CUDA execution provider
    }
    else {
        if (useGPU) {
            std::cout << "GPU is not supported by your ONNXRuntime build. Fallback to CPU." << std::endl;
        }
        std::cout << "Inference device: CPU" << std::endl;
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

    // Load class names and generate corresponding colors
    classNames = utils::getClassNames(labelsPath);
    classColors = utils::generateColorsObb(classNames);

    std::cout << "Model loaded successfully with " << numInputNodes << " input nodes and " << numOutputNodes << " output nodes." << std::endl;
}

// Preprocess function implementation
cv::Mat YOLO11OBBDetector::preprocess(const cv::Mat& image, float*& blob, std::vector<int64_t>& inputTensorShape) {
    ScopedTimer timer("preprocessing");

    cv::Mat resizedImage;
    // Resize and pad the image using letterBox utility
    utils::letterBoxObb(image, resizedImage, inputImageShape, cv::Scalar(114, 114, 114), isDynamicInputShape, false, true, 32);

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

    DEBUG_PRINT("Preprocessing completed")

        return resizedImage;
}

std::vector<ObbDetection> YOLO11OBBDetector::postprocess(
    const cv::Size& originalImageSize,
    const cv::Size& resizedImageShape,
    const std::vector<Ort::Value>& outputTensors,
    float confThreshold,
    float iouThreshold,
    int topk)
{
    ScopedTimer timer("postprocessing");
    std::vector<ObbDetection> detections;

    // Get raw output data and shape (assumed [1, num_features, num_detections])
    const float* rawOutput = outputTensors[0].GetTensorData<float>();
    const std::vector<int64_t> outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
    int num_features = static_cast<int>(outputShape[1]);
    int num_detections = static_cast<int>(outputShape[2]);
    if (num_detections == 0) {
        return detections;
    }

    // Determine number of labels/classes (layout: [x, y, w, h, scores..., angle])
    int num_labels = num_features - 5;
    if (num_labels <= 0) {
        return detections;
    }

    // Compute letterbox parameters.
    float inp_w = static_cast<float>(resizedImageShape.width);
    float inp_h = static_cast<float>(resizedImageShape.height);
    float orig_w = static_cast<float>(originalImageSize.width);
    float orig_h = static_cast<float>(originalImageSize.height);
    float r = std::min(inp_h / orig_h, inp_w / orig_w);
    int padw = std::round(orig_w * r);
    int padh = std::round(orig_h * r);
    float dw = (inp_w - padw) / 2.0f;
    float dh = (inp_h - padh) / 2.0f;
    float ratio = 1.0f / r;

    // Wrap raw output data into a cv::Mat and transpose it.
    // After transposition, each row corresponds to one detection with layout:
    // [x, y, w, h, score_0, score_1, …, score_(num_labels-1), angle]
    cv::Mat output = cv::Mat(num_features, num_detections, CV_32F, const_cast<float*>(rawOutput));
    output = output.t(); // Now shape: [num_detections, num_features]

    // Extract detections without clamping.
    std::vector<OrientedBoundingBox> obbs;
    std::vector<float> scores;
    std::vector<int> labels;
    for (int i = 0; i < num_detections; ++i) {
        float* row_ptr = output.ptr<float>(i);
        // Extract raw bbox parameters in letterbox coordinate space.
        float x = row_ptr[0];
        float y = row_ptr[1];
        float w = row_ptr[2];
        float h = row_ptr[3];

        // Extract class scores and determine the best class.
        float* scores_ptr = row_ptr + 4;
        float maxScore = -FLT_MAX;
        int classId = -1;
        for (int j = 0; j < num_labels; j++) {
            float score = scores_ptr[j];
            if (score > maxScore) {
                maxScore = score;
                classId = j;
            }
        }

        // Angle is stored right after the scores.
        float angle = row_ptr[4 + num_labels];

        if (maxScore > confThreshold) {
            // Correct the box coordinates with letterbox offsets and scaling.
            float cx = (x - dw) * ratio;
            float cy = (y - dh) * ratio;
            float bw = w * ratio;
            float bh = h * ratio;

            OrientedBoundingBox obb(cx, cy, bw, bh, angle);
            obbs.push_back(obb);
            scores.push_back(maxScore);
            labels.push_back(classId);
        }
    }

    // Combine detections into a vector<Detection> for NMS.
    std::vector<ObbDetection> detectionsForNMS;
    for (size_t i = 0; i < obbs.size(); i++) {
        detectionsForNMS.emplace_back(ObbDetection{ obbs[i], scores[i], labels[i] });
    }

    for (auto& det : detectionsForNMS) {
        det.box.x = std::min(std::max(det.box.x, 0.f), orig_w);
        det.box.y = std::min(std::max(det.box.y, 0.f), orig_h);
        det.box.width = std::min(std::max(det.box.width, 0.f), orig_w);
        det.box.height = std::min(std::max(det.box.height, 0.f), orig_h);
    }

    // Perform rotated NMS.
    std::vector<ObbDetection> post_nms_detections = utils::nonMaxSuppression(
        detectionsForNMS, confThreshold, iouThreshold, topk);



    DEBUG_PRINT("Postprocessing completed");
    return post_nms_detections;
}



// Detect function implementation
std::vector<ObbDetection> YOLO11OBBDetector::detect(const cv::Mat& image, float confThreshold, float iouThreshold) {
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

    std::vector<ObbDetection> detections = postprocess(image.size(), resizedImageShape, outputTensors, confThreshold, iouThreshold, 100);

    return detections; // Return the vector of detections
}


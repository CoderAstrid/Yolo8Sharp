#ifndef __YOLO11_COMMON_H__
#define __YOLO11_COMMON_H__

#include "yolo_define.h"
#include <opencv2/opencv.hpp>
#include <numeric>

// Struct to represent a bounding box
//struct BoundingBox {
//    int x;
//    int y;
//    int width;
//    int height;
//
//    BoundingBox() : x(0), y(0), width(0), height(0) {}
//    BoundingBox(int x_, int y_, int width_, int height_)
//        : x(x_), y(y_), width(width_), height(height_) {}
//};

/**
 * @brief Struct representing a bounding box for object detection.
 *
 * Stores the coordinates and dimensions of a detected object within an image.
 */
 // ============================================================================
 // Structs
 // ============================================================================
struct BoundingBox {
    int x{ 0 };
    int y{ 0 };
    int width{ 0 };
    int height{ 0 };

    BoundingBox() = default;
    BoundingBox(int _x, int _y, int w, int h)
        : x(_x), y(_y), width(w), height(h) {}

    float area() const { return static_cast<float>(width * height); }

    BoundingBox intersect(const BoundingBox& other) const {
        int xStart = std::max(x, other.x);
        int yStart = std::max(y, other.y);
        int xEnd = std::min(x + width, other.x + other.width);
        int yEnd = std::min(y + height, other.y + other.height);
        int iw = std::max(0, xEnd - xStart);
        int ih = std::max(0, yEnd - yStart);
        return BoundingBox(xStart, yStart, iw, ih);
    }
};

 /**
  * @brief Struct to represent a detection.
  */
struct Detection {
    BoundingBox box;
    float conf{};
    int classId{};
};

/**
 * @brief Struct to represent a classification result.
 */
struct ClassificationResult {
    int classId{ -1 };        // Predicted class ID, initialized to -1 for easier error checking
    float confidence{ 0.0f }; // Confidence score for the prediction
    String className{}; // Name of the predicted class

    ClassificationResult() = default;
    ClassificationResult(int id, float conf, String name)
        : classId(id), confidence(conf), className(std::move(name)) {}
};

/**
 * @brief Struct to represent an Oriented bounding box (OBB) in xywhr format.
 */
struct OrientedBoundingBox {
    float x;       // x-coordinate of the center
    float y;       // y-coordinate of the center
    float width;   // width of the box
    float height;  // height of the box
    float angle;   // rotation angle in radians

    OrientedBoundingBox() : x(0), y(0), width(0), height(0), angle(0) {}
    OrientedBoundingBox(float x_, float y_, float width_, float height_, float angle_)
        : x(x_), y(y_), width(width_), height(height_), angle(angle_) {}
};

/**
 * @brief Struct to represent a detection with an oriented bounding box.
 */
struct ObbDetection {
    OrientedBoundingBox box;  // Oriented bounding box in xywhr format
    float conf{};             // Confidence score
    int classId{};            // Class ID

    ObbDetection() = default;
    ObbDetection(const OrientedBoundingBox& box_, float conf_, int classId_)
        : box(box_), conf(conf_), classId(classId_) {}
};


/**
 * @brief Struct representing a detected keypoint in pose estimation.
 *
 * This struct holds the x and y coordinates of a keypoint along with
 * its confidence score, indicating the model's certainty in the prediction.
 */
struct KeyPoint {
    float x;         ///< X-coordinate of the keypoint
    float y;         ///< Y-coordinate of the keypoint
    float confidence; ///< Confidence score of the keypoint

    /**
     * @brief Constructor to initialize a KeyPoint.
     *
     * @param x_ X-coordinate of the keypoint.
     * @param y_ Y-coordinate of the keypoint.
     * @param conf_ Confidence score of the keypoint.
     */
    KeyPoint(float x_ = 0, float y_ = 0, float conf_ = 0)
        : x(x_), y(y_), confidence(conf_) {}
};



struct Segmentation {
    BoundingBox box;
    float       conf{ 0.f };
    int         classId{ 0 };
    cv::Mat     mask;  // Single-channel (8UC1) mask in full resolution
};

/**
 * @brief Struct representing a detected object in an image.
 *
 * This struct contains the bounding box, confidence score, class ID,
 * and keypoints (if applicable for pose estimation).
 */
struct PoseDetection {
    BoundingBox box;           ///< Bounding box of the detected object
    float conf{};              ///< Confidence score of the detection
    int classId{};             ///< ID of the detected class
    std::vector<KeyPoint> keypoints; ///< List of keypoints (for pose estimation)
};

static constexpr float EPS = 1e-7f;

namespace utils {
    template <typename T>
    T clamp(const T& val, const T& low, const T& high) {
        return std::max(low, std::min(val, high));
    }

    /**
     * @brief Scales detection coordinates back to the original image size.
     *
     * @param imageShape Shape of the resized image used for inference.
     * @param bbox Detection bounding box to be scaled.
     * @param imageOriginalShape Original image size before resizing.
     * @param p_Clip Whether to clip the coordinates to the image boundaries.
     * @return BoundingBox Scaled bounding box.
     */
    BoundingBox scaleCoords(
        const cv::Size& imageShape, 
        BoundingBox coords,
        const cv::Size& imageOriginalShape, 
        bool p_Clip
    );

    BoundingBox scaleCoordsSeg(const cv::Size& letterboxShape,
        const BoundingBox& coords,
        const cv::Size& originalShape,
        bool p_Clip = true);

    /**
     * @brief Computes the product of elements in a vector.
     */
    size_t vectorProduct(const std::vector<int64_t>& vector);

    cv::Mat sigmoid(const cv::Mat& src);

    /**
     * @brief Loads class names from a given file path.
     *
     * @param path Path to the file containing class names.
     * @return std::vector<std::string> Vector of class names.
     */
    std::vector<String> getClassNames(const String& path);

    /**
     * @brief Performs Non-Maximum Suppression (NMS) on the bounding boxes.
     *
     * @param boundingBoxes Vector of bounding boxes.
     * @param scores Vector of confidence scores corresponding to each bounding box.
     * @param scoreThreshold Confidence threshold to filter boxes.
     * @param nmsThreshold IoU threshold for NMS.
     * @param indices Output vector of indices that survive NMS.
     */
     // Optimized Non-Maximum Suppression Function
    void NMSBoxes(const std::vector<BoundingBox>& boundingBoxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices);

    /**
     * @brief Performs Non-Maximum Suppression (NMS) on the bounding boxes.
     *
     * @param boundingBoxes Vector of bounding boxes.
     * @param scores Vector of confidence scores corresponding to each bounding box.
     * @param scoreThreshold Confidence threshold to filter boxes.
     * @param nmsThreshold IoU threshold for NMS.
     * @param indices Output vector of indices that survive NMS.
     */
     // Optimized Non-Maximum Suppression Function
    void NMSBoxesPose(
        const std::vector<BoundingBox>& boundingBoxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices);

    void NMSBoxesSeg(
        const std::vector<BoundingBox>& boxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices
    );

    /**
     * @brief Applies NMS to detections and returns filtered results.
     * @param input_detections Input detections.
     * @param conf_thres Confidence threshold.
     * @param iou_thres IoU threshold.
     * @param max_det Maximum detections to keep.
     * @return Filtered detections after NMS.
     */
    std::vector<ObbDetection> nonMaxSuppression(
        const std::vector<ObbDetection>& input_detections,
        float conf_thres = 0.25f,
        float iou_thres = 0.75f,
        int max_det = 1000);

    /**
     * @brief Main NMS function for rotated boxes.
     * @param boxes Input boxes.
     * @param scores Confidence scores.
     * @param threshold IoU threshold.
     * @return Indices of boxes to keep.
     */
    std::vector<int> nmsRotated(const std::vector<OrientedBoundingBox>& boxes, const std::vector<float>& scores, float threshold = 0.75f);

    /**
     * @brief Performs rotated NMS on sorted OBBs.
     * @param sorted_boxes Boxes sorted by confidence.
     * @param iou_thres Threshold for IoU suppression.
     * @return Indices of boxes to keep.
     */
    std::vector<int> nmsRotatedImpl(const std::vector<OrientedBoundingBox>& sorted_boxes, float iou_thres);

    /**
     * @brief Computes IoU matrix between two sets of OBBs.
     * @param obb1 First set of OBBs.
     * @param obb2 Second set of OBBs.
     * @param eps Small constant for numerical stability.
     * @return 2D vector of IoU values.
     */
    std::vector<std::vector<float>> batchProbiou(const std::vector<OrientedBoundingBox>& obb1, const std::vector<OrientedBoundingBox>& obb2, float eps = EPS);
    

    /**
     * @brief Computes covariance matrix components for a single OBB.
     * @param box Input oriented bounding box.
     * @param out1 First component (a).
     * @param out2 Second component (b).
     * @param out3 Third component (c).
     */
    void getCovarianceComponents(const OrientedBoundingBox& box, float& out1, float& out2, float& out3);

    /**
     * @brief Resizes an image with letterboxing to maintain aspect ratio.
     *
     * @param image Input image.
     * @param outImage Output resized and padded image.
     * @param newShape Desired output size.
     * @param color Padding color (default is gray).
     * @param auto_ Automatically adjust padding to be multiple of stride.
     * @param scaleFill Whether to scale to fill the new shape without keeping aspect ratio.
     * @param scaleUp Whether to allow scaling up of the image.
     * @param stride Stride size for padding alignment.
     */
    void letterBoxObb(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color = cv::Scalar(114, 114, 114),
        bool auto_ = true,
        bool scaleFill = false,
        bool scaleUp = true,
        int stride = 32
    );

    /**
     * @brief Resizes an image with letterboxing to maintain aspect ratio.
     *
     * @param image Input image.
     * @param outImage Output resized and padded image.
     * @param newShape Desired output size.
     * @param color Padding color (default is gray).
     * @param auto_ Automatically adjust padding to be multiple of stride.
     * @param scaleFill Whether to scale to fill the new shape without keeping aspect ratio.
     * @param scaleUp Whether to allow scaling up of the image.
     * @param stride Stride size for padding alignment.
     */
    void letterBoxPos(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color = cv::Scalar(114, 114, 114),
        bool auto_ = true,
        bool scaleFill = false,
        bool scaleUp = true,
        int stride = 32
    );

    /**
     * @brief Resizes an image with letterboxing to maintain aspect ratio.
     *
     * @param image Input image.
     * @param outImage Output resized and padded image.
     * @param newShape Desired output size.
     * @param color Padding color (default is gray).
     * @param auto_ Automatically adjust padding to be multiple of stride.
     * @param scaleFill Whether to scale to fill the new shape without keeping aspect ratio.
     * @param scaleUp Whether to allow scaling up of the image.
     * @param stride Stride size for padding alignment.
     */
    void letterBox(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color = cv::Scalar(114, 114, 114),
        bool auto_ = true,
        bool scaleFill = false,
        bool scaleUp = true,
        int stride = 32
    );

    inline void letterBoxSeg(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color = cv::Scalar(114, 114, 114),
        bool auto_ = true,
        bool scaleFill = false,
        bool scaleUp = true,
        int stride = 32
    );

    /**
     * @brief Generates a vector of colors for each class name.
     *
     * @param classNames Vector of class names.
     * @param seed Seed for random color generation to ensure reproducibility.
     * @return std::vector<cv::Scalar> Vector of colors.
     */
    std::vector<cv::Scalar> generateColors(
        const std::vector<String>& classNames,
        int seed = 42
    );

    /**
     * @brief Generates a vector of colors for each class name.
     *
     * @param classNames Vector of class names.
     * @param seed Seed for random color generation to ensure reproducibility.
     * @return std::vector<cv::Scalar> Vector of colors.
     */
    std::vector<cv::Scalar> generateColorsObb(const std::vector<String>& classNames, int seed = 42);

    std::vector<cv::Scalar> generateColorsSeg(const std::vector<String>& classNames, int seed = 42);

    /**
     * @brief Prepares an image for model input by resizing and padding (letterboxing style) or simple resize.
     */
    void preprocessImageToTensor(
        const cv::Mat& image, 
        cv::Mat& outImage,
        const cv::Size& targetShape,
        const cv::Scalar& color = cv::Scalar(0, 0, 0),
        bool scaleUp = true,
        const std::string& strategy = "resize"
    );

    /**
     * @brief Draws pose estimations including bounding boxes, keypoints, and skeleton
     *
     * @param image Input/output image
     * @param detections Vector of pose detections
     * @param confidenceThreshold Minimum confidence to visualize
     * @param kptRadius Radius of keypoint circles
     * @param kptThreshold Minimum keypoint confidence to draw
     */
    void drawPoseEstimation(cv::Mat& image,
        const std::vector<PoseDetection>& detections,
        float confidenceThreshold = 0.5,
        float kptThreshold = 0.5);
}

#endif//__YOLO11_COMMON_H__
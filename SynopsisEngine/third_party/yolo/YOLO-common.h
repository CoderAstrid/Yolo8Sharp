#ifndef __YOLO11_COMMON_H__
#define __YOLO11_COMMON_H__

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
    std::string className{}; // Name of the predicted class

    ClassificationResult() = default;
    ClassificationResult(int id, float conf, std::string name)
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
    BoundingBox scaleCoords(const cv::Size& imageShape, BoundingBox coords,
        const cv::Size& imageOriginalShape, bool p_Clip) {
        BoundingBox result;
        float gain = std::min(static_cast<float>(imageShape.height) / static_cast<float>(imageOriginalShape.height),
            static_cast<float>(imageShape.width) / static_cast<float>(imageOriginalShape.width));

        int padX = static_cast<int>(std::round((imageShape.width - imageOriginalShape.width * gain) / 2.0f));
        int padY = static_cast<int>(std::round((imageShape.height - imageOriginalShape.height * gain) / 2.0f));

        result.x = static_cast<int>(std::round((coords.x - padX) / gain));
        result.y = static_cast<int>(std::round((coords.y - padY) / gain));
        result.width = static_cast<int>(std::round(coords.width / gain));
        result.height = static_cast<int>(std::round(coords.height / gain));

        if (p_Clip) {
            result.x = clamp(result.x, 0, imageOriginalShape.width);
            result.y = clamp(result.y, 0, imageOriginalShape.height);
            result.width = clamp(result.width, 0, imageOriginalShape.width - result.x);
            result.height = clamp(result.height, 0, imageOriginalShape.height - result.y);
        }
        return result;
    }

    /**
     * @brief Computes the product of elements in a vector.
     */
    size_t vectorProduct(const std::vector<int64_t>& vector) {
        if (vector.empty())
            return 0;
        return std::accumulate(vector.begin(), vector.end(), 1LL, std::multiplies<int64_t>());
    }
}

#endif//__YOLO11_COMMON_H__
#ifndef __YOLO_DEFINE_H__
#define __YOLO_DEFINE_H__
#include <string>



typedef enum YoloTask {
	YT_DETECT = 0,
	YT_CLASSIFY,
	YT_SEGMENT,
	YT_POSE,
	YT_OBB,
	YT_MAX
}yoloTask;

#if defined(UNICODE)
typedef std::wstring JString;
#else
typedef std::string JString;
#endif

/**
 * @brief Struct representing a bounding box for object detection.
 *
 * Stores the coordinates and dimensions of a detected object within an image.
 */
 // ============================================================================
 // Structs
 // ============================================================================
typedef struct BoundingBox {
    int x{ 0 };
    int y{ 0 };
    int width{ 0 };
    int height{ 0 };

    BoundingBox() = default;
    BoundingBox(int _x, int _y, int w, int h)
        : x(_x), y(_y), width(w), height(h) {}

    // float area() const { return static_cast<float>(width * height); }

    /*BoundingBox intersect(const BoundingBox& other) const {
        int xStart = std::max(x, other.x);
        int yStart = std::max(y, other.y);
        int xEnd = std::min(x + width, other.x + other.width);
        int yEnd = std::min(y + height, other.y + other.height);
        int iw = std::max(0, xEnd - xStart);
        int ih = std::max(0, yEnd - yStart);
        return BoundingBox(xStart, yStart, iw, ih);
    }*/
}tagBox;

/**
 * @brief Struct to represent a detection.
 */
typedef struct Detection {
    BoundingBox box;
    float conf{ 0.0f };
    int classId{ -1 };
}tagDetRes;

#endif//__YOLO_DEFINE_H__
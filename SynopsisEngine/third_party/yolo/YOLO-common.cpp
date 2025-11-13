#include "YOLO-common.h"

#include <random>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>
#include <memory>
#include <chrono>
#include <numeric>
#include <unordered_map>
#include <thread>

/**
 * @brief List of COCO skeleton connections for human pose estimation.
 *
 * Defines the connections between keypoints in a skeleton structure using
 * 0-based indices, which are standard in COCO pose datasets.
 */
const std::vector<std::pair<int, int>> POSE_SKELETON = {
    // Face connections
    {0,1}, {0,2}, {1,3}, {2,4},
    // Head-to-shoulder connections
    {3,5}, {4,6},
    // Arms
    {5,7}, {7,9}, {6,8}, {8,10},
    // Body
    {5,6}, {5,11}, {6,12}, {11,12},
    // Legs
    {11,13}, {13,15}, {12,14}, {14,16}
};

namespace utils {
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

    BoundingBox scaleCoordsSeg(
        const cv::Size& letterboxShape,
        const BoundingBox& coords,
        const cv::Size& originalShape,
        bool p_Clip
    ) {
        float gain = std::min((float)letterboxShape.height / (float)originalShape.height,
            (float)letterboxShape.width / (float)originalShape.width);

        int padW = static_cast<int>(std::round(((float)letterboxShape.width - (float)originalShape.width * gain) / 2.f));
        int padH = static_cast<int>(std::round(((float)letterboxShape.height - (float)originalShape.height * gain) / 2.f));

        BoundingBox ret;
        ret.x = static_cast<int>(std::round(((float)coords.x - (float)padW) / gain));
        ret.y = static_cast<int>(std::round(((float)coords.y - (float)padH) / gain));
        ret.width = static_cast<int>(std::round((float)coords.width / gain));
        ret.height = static_cast<int>(std::round((float)coords.height / gain));

        if (p_Clip) {
            ret.x = clamp(ret.x, 0, originalShape.width);
            ret.y = clamp(ret.y, 0, originalShape.height);
            ret.width = clamp(ret.width, 0, originalShape.width - ret.x);
            ret.height = clamp(ret.height, 0, originalShape.height - ret.y);
        }

        return ret;
    }

    size_t vectorProduct(const std::vector<int64_t>& vector) {
        if (vector.empty())
            return 0;
        return std::accumulate(vector.begin(), vector.end(), 1LL, std::multiplies<int64_t>());
    }

    cv::Mat sigmoid(const cv::Mat& src) 
    {
        cv::Mat dst;
        cv::exp(-src, dst);
        dst = 1.0 / (1.0 + dst);
        return dst;
    }

    std::vector<JString>
        getClassNames(const JString& path
    ) {
        std::vector<JString> classNames;
#if defined(UNICODE)
        std::wifstream infile(path);
#else
        std::ifstream infile(path);
#endif

        if (infile) {
            JString line;
            while (getline(infile, line)) {
                // Remove carriage return if present (for Windows compatibility)
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                classNames.emplace_back(line);
            }
        }
        else {
            std::wcerr << L"ERROR: Failed to access class name path: " << path << std::endl;
        }

        // DEBUG_PRINT("Loaded " << classNames.size() << " class names from " + path);
        return classNames;
    }

    void NMSBoxes(const std::vector<BoundingBox>& boundingBoxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices)
    {
        indices.clear();

        const size_t numBoxes = boundingBoxes.size();
        if (numBoxes == 0) {
            // DEBUG_PRINT("No bounding boxes to process in NMS");
            return;
        }

        // Step 1: Filter out boxes with scores below the threshold
        // and create a list of indices sorted by descending scores
        std::vector<int> sortedIndices;
        sortedIndices.reserve(numBoxes);
        for (size_t i = 0; i < numBoxes; ++i) {
            if (scores[i] >= scoreThreshold) {
                sortedIndices.push_back(static_cast<int>(i));
            }
        }

        // If no boxes remain after thresholding
        if (sortedIndices.empty()) {
            // DEBUG_PRINT("No bounding boxes above score threshold");
            return;
        }

        // Sort the indices based on scores in descending order
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&scores](int idx1, int idx2) {
                return scores[idx1] > scores[idx2];
            });

        // Step 2: Precompute the areas of all boxes
        std::vector<float> areas(numBoxes, 0.0f);
        for (size_t i = 0; i < numBoxes; ++i) {
            areas[i] = boundingBoxes[i].width * boundingBoxes[i].height;
        }

        // Step 3: Suppression mask to mark boxes that are suppressed
        std::vector<bool> suppressed(numBoxes, false);

        // Step 4: Iterate through the sorted list and suppress boxes with high IoU
        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            int currentIdx = sortedIndices[i];
            if (suppressed[currentIdx]) {
                continue;
            }

            // Select the current box as a valid detection
            indices.push_back(currentIdx);

            const BoundingBox& currentBox = boundingBoxes[currentIdx];
            const float x1_max = currentBox.x;
            const float y1_max = currentBox.y;
            const float x2_max = currentBox.x + currentBox.width;
            const float y2_max = currentBox.y + currentBox.height;
            const float area_current = areas[currentIdx];

            // Compare IoU of the current box with the rest
            for (size_t j = i + 1; j < sortedIndices.size(); ++j) {
                int compareIdx = sortedIndices[j];
                if (suppressed[compareIdx]) {
                    continue;
                }

                const BoundingBox& compareBox = boundingBoxes[compareIdx];
                const float x1 = std::max(x1_max, static_cast<float>(compareBox.x));
                const float y1 = std::max(y1_max, static_cast<float>(compareBox.y));
                const float x2 = std::min(x2_max, static_cast<float>(compareBox.x + compareBox.width));
                const float y2 = std::min(y2_max, static_cast<float>(compareBox.y + compareBox.height));

                const float interWidth = x2 - x1;
                const float interHeight = y2 - y1;

                if (interWidth <= 0 || interHeight <= 0) {
                    continue;
                }

                const float intersection = interWidth * interHeight;
                const float unionArea = area_current + areas[compareIdx] - intersection;
                const float iou = (unionArea > 0.0f) ? (intersection / unionArea) : 0.0f;

                if (iou > nmsThreshold) {
                    suppressed[compareIdx] = true;
                }
            }
        }

        // DEBUG_PRINT("NMS completed with " + std::to_string(indices.size()) + " indices remaining");
    }

    void NMSBoxesPose(const std::vector<BoundingBox>& boundingBoxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices)
    {
        indices.clear();

        const size_t numBoxes = boundingBoxes.size();
        if (numBoxes == 0) {
            // DEBUG_PRINT("No bounding boxes to process in NMS");
            return;
        }

        // Step 1: Filter out boxes with scores below the threshold
        // and create a list of indices sorted by descending scores
        std::vector<int> sortedIndices;
        sortedIndices.reserve(numBoxes);
        for (size_t i = 0; i < numBoxes; ++i) {
            if (scores[i] >= scoreThreshold) {
                sortedIndices.push_back(static_cast<int>(i));
            }
        }

        // If no boxes remain after thresholding
        if (sortedIndices.empty()) {
            // DEBUG_PRINT("No bounding boxes above score threshold");
            return;
        }

        // Sort the indices based on scores in descending order
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&scores](int idx1, int idx2) {
                return scores[idx1] > scores[idx2];
            });

        // Step 2: Precompute the areas of all boxes
        std::vector<float> areas(numBoxes, 0.0f);
        for (size_t i = 0; i < numBoxes; ++i) {
            areas[i] = boundingBoxes[i].width * boundingBoxes[i].height;
        }

        // Step 3: Suppression mask to mark boxes that are suppressed
        std::vector<bool> suppressed(numBoxes, false);

        // Step 4: Iterate through the sorted list and suppress boxes with high IoU
        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            int currentIdx = sortedIndices[i];
            if (suppressed[currentIdx]) {
                continue;
            }

            // Select the current box as a valid detection
            indices.push_back(currentIdx);

            const BoundingBox& currentBox = boundingBoxes[currentIdx];
            const float x1_max = currentBox.x;
            const float y1_max = currentBox.y;
            const float x2_max = currentBox.x + currentBox.width;
            const float y2_max = currentBox.y + currentBox.height;
            const float area_current = areas[currentIdx];

            // Compare IoU of the current box with the rest
            for (size_t j = i + 1; j < sortedIndices.size(); ++j) {
                int compareIdx = sortedIndices[j];
                if (suppressed[compareIdx]) {
                    continue;
                }

                const BoundingBox& compareBox = boundingBoxes[compareIdx];
                const float x1 = std::max(x1_max, static_cast<float>(compareBox.x));
                const float y1 = std::max(y1_max, static_cast<float>(compareBox.y));
                const float x2 = std::min(x2_max, static_cast<float>(compareBox.x + compareBox.width));
                const float y2 = std::min(y2_max, static_cast<float>(compareBox.y + compareBox.height));

                const float interWidth = x2 - x1;
                const float interHeight = y2 - y1;

                if (interWidth <= 0 || interHeight <= 0) {
                    continue;
                }

                const float intersection = interWidth * interHeight;
                const float unionArea = area_current + areas[compareIdx] - intersection;
                const float iou = (unionArea > 0.0f) ? (intersection / unionArea) : 0.0f;

                if (iou > nmsThreshold) {
                    suppressed[compareIdx] = true;
                }
            }
        }

        // DEBUG_PRINT("NMS completed with " + std::to_string(indices.size()) + " indices remaining");
    }

    void NMSBoxesSeg(
        const std::vector<BoundingBox>& boxes,
        const std::vector<float>& scores,
        float scoreThreshold,
        float nmsThreshold,
        std::vector<int>& indices
    ) {
        indices.clear();
        if (boxes.empty()) {
            return;
        }

        std::vector<int> order;
        order.reserve(boxes.size());
        for (size_t i = 0; i < boxes.size(); ++i) {
            if (scores[i] >= scoreThreshold) {
                order.push_back((int)i);
            }
        }
        if (order.empty())
            return;

        std::sort(order.begin(), order.end(),
            [&scores](int a, int b) {
                return scores[a] > scores[b];
            });

        std::vector<float> areas(boxes.size());
        for (size_t i = 0; i < boxes.size(); ++i) {
            areas[i] = (float)(boxes[i].width * boxes[i].height);
        }

        std::vector<bool> suppressed(boxes.size(), false);
        for (size_t i = 0; i < order.size(); ++i) {
            int idx = order[i];
            if (suppressed[idx])
                continue;

            indices.push_back(idx);

            for (size_t j = i + 1; j < order.size(); ++j) {
                int idx2 = order[j];
                if (suppressed[idx2])
                    continue;

                const BoundingBox& a = boxes[idx];
                const BoundingBox& b = boxes[idx2];
                int interX1 = std::max(a.x, b.x);
                int interY1 = std::max(a.y, b.y);
                int interX2 = std::min(a.x + a.width, b.x + b.width);
                int interY2 = std::min(a.y + a.height, b.y + b.height);

                int w = interX2 - interX1;
                int h = interY2 - interY1;
                if (w > 0 && h > 0) {
                    float interArea = (float)(w * h);
                    float unionArea = areas[idx] + areas[idx2] - interArea;
                    float iou = (unionArea > 0.f) ? (interArea / unionArea) : 0.f;
                    if (iou > nmsThreshold) {
                        suppressed[idx2] = true;
                    }
                }
            }
        }
    }

    std::vector<ObbDetection> nonMaxSuppression(
        const std::vector<ObbDetection>& input_detections,
        float conf_thres,
        float iou_thres,
        int max_det
    ) {

        // Filter by confidence
        std::vector<ObbDetection> candidates;
        for (const auto& det : input_detections) {
            if (det.conf > conf_thres) {
                candidates.push_back(det);
            }
        }
        if (candidates.empty()) return {};

        // Extract boxes and scores
        std::vector<OrientedBoundingBox> boxes;
        std::vector<float> scores;
        for (const auto& det : candidates) {
            boxes.push_back(det.box);
            scores.push_back(det.conf);
        }

        // Run NMS
        std::vector<int> keep_indices = nmsRotated(boxes, scores, iou_thres);

        // Collect results
        std::vector<ObbDetection> results;
        for (int idx : keep_indices) {
            if (results.size() >= max_det) break;
            results.push_back(candidates[idx]);
        }
        return results;
    }

    /**
     * @brief Main NMS function for rotated boxes.
     * @param boxes Input boxes.
     * @param scores Confidence scores.
     * @param threshold IoU threshold.
     * @return Indices of boxes to keep.
     */
    std::vector<int> nmsRotated(const std::vector<OrientedBoundingBox>& boxes, const std::vector<float>& scores, float threshold) {
        // Sort indices based on scores
        std::vector<int> indices(boxes.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&scores](int a, int b) {
            return scores[a] > scores[b];
            });

        // Create sorted boxes list
        std::vector<OrientedBoundingBox> sorted_boxes;
        for (int idx : indices) {
            sorted_boxes.push_back(boxes[idx]);
        }

        // Perform NMS
        std::vector<int> keep = nmsRotatedImpl(sorted_boxes, threshold);

        // Map back to original indices
        std::vector<int> keep_indices;
        for (int k : keep) {
            keep_indices.push_back(indices[k]);
        }
        return keep_indices;
    }

    /**
     * @brief Performs rotated NMS on sorted OBBs.
     * @param sorted_boxes Boxes sorted by confidence.
     * @param iou_thres Threshold for IoU suppression.
     * @return Indices of boxes to keep.
     */
    std::vector<int> nmsRotatedImpl(const std::vector<OrientedBoundingBox>& sorted_boxes, float iou_thres) {
        auto ious = batchProbiou(sorted_boxes, sorted_boxes);
        std::vector<int> keep;
        const int n = sorted_boxes.size();

        for (int j = 0; j < n; ++j) {
            bool keep_j = true;
            for (int i = 0; i < j; ++i) {
                if (ious[i][j] >= iou_thres) {
                    keep_j = false;
                    break;
                }
            }
            if (keep_j) keep.push_back(j);
        }
        return keep;
    }

    /**
     * @brief Computes IoU matrix between two sets of OBBs.
     * @param obb1 First set of OBBs.
     * @param obb2 Second set of OBBs.
     * @param eps Small constant for numerical stability.
     * @return 2D vector of IoU values.
     */
    std::vector<std::vector<float>> batchProbiou(const std::vector<OrientedBoundingBox>& obb1, const std::vector<OrientedBoundingBox>& obb2, float eps) {
        size_t N = obb1.size();
        size_t M = obb2.size();
        std::vector<std::vector<float>> iou_matrix(N, std::vector<float>(M, 0.0f));

        for (size_t i = 0; i < N; ++i) {
            const OrientedBoundingBox& box1 = obb1[i];
            float x1 = box1.x, y1 = box1.y;
            float a1, b1, c1;
            getCovarianceComponents(box1, a1, b1, c1);

            for (size_t j = 0; j < M; ++j) {
                const OrientedBoundingBox& box2 = obb2[j];
                float x2 = box2.x, y2 = box2.y;
                float a2, b2, c2;
                getCovarianceComponents(box2, a2, b2, c2);

                // Compute denominator
                float denom = (a1 + a2) * (b1 + b2) - std::pow(c1 + c2, 2) + eps;

                // Terms t1 and t2
                float dx = x1 - x2;
                float dy = y1 - y2;
                float t1 = ((a1 + a2) * dy * dy + (b1 + b2) * dx * dx) * 0.25f / denom;
                float t2 = ((c1 + c2) * (x2 - x1) * dy) * 0.5f / denom;

                // Term t3
                float term1 = a1 * b1 - c1 * c1;
                term1 = std::max(term1, 0.0f);
                float term2 = a2 * b2 - c2 * c2;
                term2 = std::max(term2, 0.0f);
                float sqrt_term = std::sqrt(term1 * term2);

                float numerator = (a1 + a2) * (b1 + b2) - std::pow(c1 + c2, 2);
                float denominator_t3 = 4.0f * sqrt_term + eps;
                float t3 = 0.5f * std::log(numerator / denominator_t3 + eps);

                // Compute final IoU
                float bd = t1 + t2 + t3;
                bd = std::clamp(bd, eps, 100.0f);
                float hd = std::sqrt(1.0f - std::exp(-bd) + eps);
                iou_matrix[i][j] = 1.0f - hd;
            }
        }
        return iou_matrix;
    }

    /**
     * @brief Computes covariance matrix components for a single OBB.
     * @param box Input oriented bounding box.
     * @param out1 First component (a).
     * @param out2 Second component (b).
     * @param out3 Third component (c).
     */
    void getCovarianceComponents(const OrientedBoundingBox& box, float& out1, float& out2, float& out3) {
        float a = (box.width * box.width) / 12.0f;
        float b = (box.height * box.height) / 12.0f;
        float angle = box.angle;

        float cos_theta = std::cos(angle);
        float sin_theta = std::sin(angle);
        float cos_sq = cos_theta * cos_theta;
        float sin_sq = sin_theta * sin_theta;

        out1 = a * cos_sq + b * sin_sq;
        out2 = a * sin_sq + b * cos_sq;
        out3 = (a - b) * cos_theta * sin_theta;
    }

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
        const cv::Scalar& color,
        bool auto_,
        bool scaleFill,
        bool scaleUp,
        int stride
    ) {
        // Calculate the scaling ratio to fit the image within the new shape
        float ratio = std::min(static_cast<float>(newShape.height) / image.rows,
            static_cast<float>(newShape.width) / image.cols);

        // Prevent scaling up if not allowed
        if (!scaleUp) {
            ratio = std::min(ratio, 1.0f);
        }

        // Calculate new dimensions after scaling
        int newUnpadW = static_cast<int>(std::round(image.cols * ratio));
        int newUnpadH = static_cast<int>(std::round(image.rows * ratio));

        // Calculate padding needed to reach the desired shape
        int dw = newShape.width - newUnpadW;
        int dh = newShape.height - newUnpadH;

        if (auto_) {
            // Ensure padding is a multiple of stride for model compatibility
            dw = (dw % stride) / 2;
            dh = (dh % stride) / 2;
        }
        else if (scaleFill) {
            // Scale to fill without maintaining aspect ratio
            newUnpadW = newShape.width;
            newUnpadH = newShape.height;
            ratio = std::min(static_cast<float>(newShape.width) / image.cols,
                static_cast<float>(newShape.height) / image.rows);
            dw = 0;
            dh = 0;
        }
        else {
            // Evenly distribute padding on both sides
            // Calculate separate padding for left/right and top/bottom to handle odd padding
            int padLeft = dw / 2;
            int padRight = dw - padLeft;
            int padTop = dh / 2;
            int padBottom = dh - padTop;

            // Resize the image if the new dimensions differ
            if (image.cols != newUnpadW || image.rows != newUnpadH) {
                cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
            }
            else {
                // Avoid unnecessary copying if dimensions are the same
                outImage = image;
            }

            // Apply padding to reach the desired shape
            cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
            return; // Exit early since padding is already applied
        }

        // Resize the image if the new dimensions differ
        if (image.cols != newUnpadW || image.rows != newUnpadH) {
            cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
        }
        else {
            // Avoid unnecessary copying if dimensions are the same
            outImage = image;
        }

        // Calculate separate padding for left/right and top/bottom to handle odd padding
        int padLeft = dw / 2;
        int padRight = dw - padLeft;
        int padTop = dh / 2;
        int padBottom = dh - padTop;

        // Apply padding to reach the desired shape
        cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
    }

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
        const cv::Scalar& color,
        bool auto_,
        bool scaleFill,
        bool scaleUp,
        int stride
    ) {
        // Calculate the scaling ratio to fit the image within the new shape
        float ratio = std::min(static_cast<float>(newShape.height) / image.rows,
            static_cast<float>(newShape.width) / image.cols);

        // Prevent scaling up if not allowed
        if (!scaleUp) {
            ratio = std::min(ratio, 1.0f);
        }

        // Calculate new dimensions after scaling
        int newUnpadW = static_cast<int>(std::round(image.cols * ratio));
        int newUnpadH = static_cast<int>(std::round(image.rows * ratio));

        // Calculate padding needed to reach the desired shape
        int dw = newShape.width - newUnpadW;
        int dh = newShape.height - newUnpadH;

        if (auto_) {
            // Ensure padding is a multiple of stride for model compatibility
            dw = (dw % stride) / 2;
            dh = (dh % stride) / 2;
        }
        else if (scaleFill) {
            // Scale to fill without maintaining aspect ratio
            newUnpadW = newShape.width;
            newUnpadH = newShape.height;
            ratio = std::min(static_cast<float>(newShape.width) / image.cols,
                static_cast<float>(newShape.height) / image.rows);
            dw = 0;
            dh = 0;
        }
        else {
            // Evenly distribute padding on both sides
            // Calculate separate padding for left/right and top/bottom to handle odd padding
            int padLeft = dw / 2;
            int padRight = dw - padLeft;
            int padTop = dh / 2;
            int padBottom = dh - padTop;

            // Resize the image if the new dimensions differ
            if (image.cols != newUnpadW || image.rows != newUnpadH) {
                cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
            }
            else {
                // Avoid unnecessary copying if dimensions are the same
                outImage = image;
            }

            // Apply padding to reach the desired shape
            cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
            return; // Exit early since padding is already applied
        }

        // Resize the image if the new dimensions differ
        if (image.cols != newUnpadW || image.rows != newUnpadH) {
            cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
        }
        else {
            // Avoid unnecessary copying if dimensions are the same
            outImage = image;
        }

        // Calculate separate padding for left/right and top/bottom to handle odd padding
        int padLeft = dw / 2;
        int padRight = dw - padLeft;
        int padTop = dh / 2;
        int padBottom = dh - padTop;

        // Apply padding to reach the desired shape
        cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
    }

    void letterBox(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color,
        bool auto_,
        bool scaleFill,
        bool scaleUp,
        int stride
    ) {
        // Calculate the scaling ratio to fit the image within the new shape
        float ratio = std::min(static_cast<float>(newShape.height) / image.rows,
            static_cast<float>(newShape.width) / image.cols);

        // Prevent scaling up if not allowed
        if (!scaleUp) {
            ratio = std::min(ratio, 1.0f);
        }

        // Calculate new dimensions after scaling
        int newUnpadW = static_cast<int>(std::round(image.cols * ratio));
        int newUnpadH = static_cast<int>(std::round(image.rows * ratio));

        // Calculate padding needed to reach the desired shape
        int dw = newShape.width - newUnpadW;
        int dh = newShape.height - newUnpadH;

        if (auto_) {
            // Ensure padding is a multiple of stride for model compatibility
            dw = (dw % stride) / 2;
            dh = (dh % stride) / 2;
        }
        else if (scaleFill) {
            // Scale to fill without maintaining aspect ratio
            newUnpadW = newShape.width;
            newUnpadH = newShape.height;
            ratio = std::min(static_cast<float>(newShape.width) / image.cols,
                static_cast<float>(newShape.height) / image.rows);
            dw = 0;
            dh = 0;
        }
        else {
            // Evenly distribute padding on both sides
            // Calculate separate padding for left/right and top/bottom to handle odd padding
            int padLeft = dw / 2;
            int padRight = dw - padLeft;
            int padTop = dh / 2;
            int padBottom = dh - padTop;

            // Resize the image if the new dimensions differ
            if (image.cols != newUnpadW || image.rows != newUnpadH) {
                cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
            }
            else {
                // Avoid unnecessary copying if dimensions are the same
                outImage = image;
            }

            // Apply padding to reach the desired shape
            cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
            return; // Exit early since padding is already applied
        }

        // Resize the image if the new dimensions differ
        if (image.cols != newUnpadW || image.rows != newUnpadH) {
            cv::resize(image, outImage, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);
        }
        else {
            // Avoid unnecessary copying if dimensions are the same
            outImage = image;
        }

        // Calculate separate padding for left/right and top/bottom to handle odd padding
        int padLeft = dw / 2;
        int padRight = dw - padLeft;
        int padTop = dh / 2;
        int padBottom = dh - padTop;

        // Apply padding to reach the desired shape
        cv::copyMakeBorder(outImage, outImage, padTop, padBottom, padLeft, padRight, cv::BORDER_CONSTANT, color);
    }

    void letterBoxSeg(const cv::Mat& image, cv::Mat& outImage,
        const cv::Size& newShape,
        const cv::Scalar& color,
        bool auto_,
        bool scaleFill,
        bool scaleUp,
        int stride
    ) {
        float r = std::min((float)newShape.height / (float)image.rows,
            (float)newShape.width / (float)image.cols);
        if (!scaleUp) {
            r = std::min(r, 1.0f);
        }

        int newW = static_cast<int>(std::round(image.cols * r));
        int newH = static_cast<int>(std::round(image.rows * r));

        int dw = newShape.width - newW;
        int dh = newShape.height - newH;

        if (auto_) {
            dw = dw % stride;
            dh = dh % stride;
        }
        else if (scaleFill) {
            newW = newShape.width;
            newH = newShape.height;
            dw = 0;
            dh = 0;
        }

        cv::Mat resized;
        cv::resize(image, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

        int top = dh / 2;
        int bottom = dh - top;
        int left = dw / 2;
        int right = dw - left;
        cv::copyMakeBorder(resized, outImage, top, bottom, left, right, cv::BORDER_CONSTANT, color);
    }

    std::vector<cv::Scalar> generateColors(
        const std::vector<JString>& classNames,
        int seed
    ) {
        // Static cache to store colors based on class names to avoid regenerating
        static std::unordered_map<size_t, std::vector<cv::Scalar>> colorCache;

        // Compute a hash key based on class names to identify unique class configurations
        size_t hashKey = 0;
        for (const auto& name : classNames) {
            hashKey ^= std::hash<JString>{}(name)+0x9e3779b9 + (hashKey << 6) + (hashKey >> 2);
        }

        // Check if colors for this class configuration are already cached
        auto it = colorCache.find(hashKey);
        if (it != colorCache.end()) {
            return it->second;
        }

        // Generate unique random colors for each class
        std::vector<cv::Scalar> colors;
        colors.reserve(classNames.size());

        std::mt19937 rng(seed); // Initialize random number generator with fixed seed
        std::uniform_int_distribution<int> uni(0, 255); // Define distribution for color values

        for (size_t i = 0; i < classNames.size(); ++i) {
            colors.emplace_back(cv::Scalar(uni(rng), uni(rng), uni(rng))); // Generate random BGR color
        }

        // Cache the generated colors for future use
        colorCache.emplace(hashKey, colors);

        return colorCache[hashKey];
    }

     std::vector<cv::Scalar> generateColorsObb(
         const std::vector<JString>& classNames,
         int seed
     ) {
        // Static cache to store colors based on class names to avoid regenerating
        static std::unordered_map<size_t, std::vector<cv::Scalar>> colorCache;

        // Compute a hash key based on class names to identify unique class configurations
        size_t hashKey = 0;
        for (const auto& name : classNames) {
            hashKey ^= std::hash<JString>{}(name)+0x9e3779b9 + (hashKey << 6) + (hashKey >> 2);
        }

        // Check if colors for this class configuration are already cached
        auto it = colorCache.find(hashKey);
        if (it != colorCache.end()) {
            return it->second;
        }

        // Generate unique random colors for each class
        std::vector<cv::Scalar> colors;
        colors.reserve(classNames.size());

        std::mt19937 rng(seed); // Initialize random number generator with fixed seed
        std::uniform_int_distribution<int> uni(0, 255); // Define distribution for color values

        for (size_t i = 0; i < classNames.size(); ++i) {
            colors.emplace_back(cv::Scalar(uni(rng), uni(rng), uni(rng))); // Generate random BGR color
        }

        // Cache the generated colors for future use
        colorCache.emplace(hashKey, colors);

        return colorCache[hashKey];
     }

     std::vector<cv::Scalar> generateColorsSeg(
         const std::vector<JString>& classNames,
         int seed
     ) {
         static std::unordered_map<size_t, std::vector<cv::Scalar>> cache;
         size_t key = 0;
         for (const auto& name : classNames) {
             size_t h = std::hash<JString>{}(name);
             key ^= (h + 0x9e3779b9 + (key << 6) + (key >> 2));
         }
         auto it = cache.find(key);
         if (it != cache.end()) {
             return it->second;
         }
         std::mt19937 rng(seed);
         std::uniform_int_distribution<int> dist(0, 255);
         std::vector<cv::Scalar> colors;
         colors.reserve(classNames.size());
         for (size_t i = 0; i < classNames.size(); ++i) {
             colors.emplace_back(cv::Scalar(dist(rng), dist(rng), dist(rng)));
         }
         cache[key] = colors;
         return colors;
     }


    void preprocessImageToTensor(
        const cv::Mat& image, 
        cv::Mat& outImage,
        const cv::Size& targetShape,
        const cv::Scalar& color,
        bool scaleUp,
        const std::string& strategy
    ) {
        if (image.empty()) {
            // Note: This is in a utility function, so we can't easily add Logger.h here without circular dependencies
            // Keeping std::cerr for now, or we could pass a logger callback
            std::cerr << "ERROR: Input image to preprocessImageToTensor is empty." << std::endl;
            return;
        }

        if (strategy == "letterbox") {
            float r = std::min(static_cast<float>(targetShape.height) / image.rows,
                static_cast<float>(targetShape.width) / image.cols);
            if (!scaleUp) {
                r = std::min(r, 1.0f);
            }
            int newUnpadW = static_cast<int>(std::round(image.cols * r));
            int newUnpadH = static_cast<int>(std::round(image.rows * r));

            cv::Mat resizedTemp;
            cv::resize(image, resizedTemp, cv::Size(newUnpadW, newUnpadH), 0, 0, cv::INTER_LINEAR);

            int dw = targetShape.width - newUnpadW;
            int dh = targetShape.height - newUnpadH;

            int top = dh / 2;
            int bottom = dh - top;
            int left = dw / 2;
            int right = dw - left;

            cv::copyMakeBorder(resizedTemp, outImage, top, bottom, left, right, cv::BORDER_CONSTANT, color);
        }
        else { // Default to "resize"
            if (image.size() == targetShape) {
                outImage = image.clone();
            }
            else {
                cv::resize(image, outImage, targetShape, 0, 0, cv::INTER_LINEAR);
            }
        }
    }

    /**
     * @brief Draws pose estimations including bounding boxes, keypoints, and skeleton
     *
     * @param image Input/output image
     * @param detections Vector of pose detections
     * @param confidenceThreshold Minimum confidence to visualize
     * @param kptRadius Radius of keypoint circles
     * @param kptThreshold Minimum keypoint confidence to draw
     */
    inline void drawPoseEstimation(cv::Mat& image,
        const std::vector<PoseDetection>& detections,
        float confidenceThreshold,
        float kptThreshold
    ) {
        // Calculate dynamic sizes based on image dimensions
        const int min_dim = std::min(image.rows, image.cols);
        const float scale_factor = min_dim / 1280.0f;  // Reference 1280px size

        // Dynamic sizing parameters
        const int line_thickness = std::max(1, static_cast<int>(2 * scale_factor));
        const int kpt_radius = std::max(2, static_cast<int>(4 * scale_factor));
        const float font_scale = 0.5f * scale_factor;
        const int text_thickness = std::max(1, static_cast<int>(1 * scale_factor));
        const int text_offset = static_cast<int>(10 * scale_factor);

        // Define the Ultralytics pose palette (BGR format)
        // Original RGB values: [255,128,0], [255,153,51], [255,178,102], [230,230,0], [255,153,255],
        // [153,204,255], [255,102,255], [255,51,255], [102,178,255], [51,153,255],
        // [255,153,153], [255,102,102], [255,51,51], [153,255,153], [102,255,102],
        // [51,255,51], [0,255,0], [0,0,255], [255,0,0], [255,255,255]
        // Converted to BGR:
        static const std::vector<cv::Scalar> pose_palette = {
            cv::Scalar(0,128,255),    // 0
            cv::Scalar(51,153,255),   // 1
            cv::Scalar(102,178,255),  // 2
            cv::Scalar(0,230,230),    // 3
            cv::Scalar(255,153,255),  // 4
            cv::Scalar(255,204,153),  // 5
            cv::Scalar(255,102,255),  // 6
            cv::Scalar(255,51,255),   // 7
            cv::Scalar(255,178,102),  // 8
            cv::Scalar(255,153,51),   // 9
            cv::Scalar(153,153,255),  // 10
            cv::Scalar(102,102,255),  // 11
            cv::Scalar(51,51,255),    // 12
            cv::Scalar(153,255,153),  // 13
            cv::Scalar(102,255,102),  // 14
            cv::Scalar(51,255,51),    // 15
            cv::Scalar(0,255,0),      // 16
            cv::Scalar(255,0,0),      // 17
            cv::Scalar(0,0,255),      // 18
            cv::Scalar(255,255,255)   // 19
        };

        // Define per-keypoint color indices (for keypoints 0 to 16)
        static const std::vector<int> kpt_color_indices = { 16,16,16,16,16,0,0,0,0,0,0,9,9,9,9,9,9 };
        // Define per-limb color indices for each skeleton connection.
        // Make sure the number of entries here matches the number of pairs in POSE_SKELETON.
        static const std::vector<int> limb_color_indices = { 9,9,9,9,7,7,7,0,0,0,0,0,16,16,16,16,16,16,16 };

        // Loop through each detection
        for (const auto& detection : detections) {
            if (detection.conf < confidenceThreshold)
                continue;

            // Draw bounding box (optional � remove if you prefer only pose visualization)
            const auto& box = detection.box;
            cv::rectangle(image,
                cv::Point(box.x, box.y),
                cv::Point(box.x + box.width, box.y + box.height),
                cv::Scalar(0, 255, 0), // You can change the box color if desired
                line_thickness);

            // Prepare a vector to hold keypoint positions and validity flags.
            const size_t numKpts = detection.keypoints.size();
            std::vector<cv::Point> kpt_points(numKpts, cv::Point(-1, -1));
            std::vector<bool> valid(numKpts, false);

            // Draw keypoints using the corresponding palette colors
            for (size_t i = 0; i < numKpts; i++) {
                if (detection.keypoints[i].confidence >= kptThreshold) {
                    int x = std::round(detection.keypoints[i].x);
                    int y = std::round(detection.keypoints[i].y);
                    kpt_points[i] = cv::Point(x, y);
                    valid[i] = true;
                    int color_index = (i < kpt_color_indices.size()) ? kpt_color_indices[i] : 0;
                    cv::circle(image, cv::Point(x, y), kpt_radius, pose_palette[color_index], -1, cv::LINE_AA);
                }
            }

            // Draw skeleton connections based on a predefined POSE_SKELETON (vector of pairs)
            // Make sure that POSE_SKELETON is defined with 0-indexed keypoint indices.
            for (size_t j = 0; j < POSE_SKELETON.size(); j++) {
                auto [src, dst] = POSE_SKELETON[j];
                if (src < numKpts && dst < numKpts && valid[src] && valid[dst]) {
                    // Use the corresponding limb color from the palette
                    int limb_color_index = (j < limb_color_indices.size()) ? limb_color_indices[j] : 0;
                    cv::line(image, kpt_points[src], kpt_points[dst],
                        pose_palette[limb_color_index],
                        line_thickness, cv::LINE_AA);
                }
            }

            // (Optional) Add text labels such as confidence scores here if desired.
        }
    }
}

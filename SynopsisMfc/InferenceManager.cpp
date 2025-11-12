#include "pch.h"
#include "InferenceManager.h"

void InferenceManager::start(
    FrameQueue* inputQueue,
    FrameQueue* outputQueue,
    InferenceMode mode,
    vsHandle detector
) {
    stopFlag_ = false;
    mode_ = mode;
    inputQueue_ = inputQueue;
    outputQueue_ = outputQueue;
    detector_ = detector;

    th_ = std::thread(&InferenceManager::loop, this);
}

void InferenceManager::stop() 
{
    stopFlag_ = true;
    if (th_.joinable())
        th_.join();
}

void InferenceManager::loop() 
{
    while (!stopFlag_) {
        FrameInfo frame;
        if (mode_ == InferenceMode::Realtime) {
            // Drain queue, take only latest
            while (inputQueue_->try_pop(frame)) { /* discard until last */ }
        }
        else {
            // Pop one-by-one
            inputQueue_->pop(frame);
        }

        if (frame.frame.empty())
            continue;

        // Run detection (synchronously)
        if (!detector_ || frame.frame.empty()) {
            std::cerr << "Invalid YOLO handle or empty frame!" << std::endl;
            return;
        }

        const unsigned char* imgData = frame.frame.data;
        int width = frame.frame.cols;
        int height = frame.frame.rows;
        int channels = frame.frame.channels();

        Detection* detections = nullptr;
        int count = 0;

        vsCode result = vsDetectObjects(detector_, imgData, width, height, channels, &detections, &count);

        if (result == VS_SUCCESS && detections != nullptr && count > 0) {
            for (int i = 0; i < count; ++i) {
                const Detection& det = detections[i];                
            }

            // Don't forget to free the detection array if allocated in DLL
            delete[] detections;
        }
        else {
            std::cerr << "Detection failed or no objects detected!" << std::endl;
        }

        outputQueue_->push(frame);
    }
}
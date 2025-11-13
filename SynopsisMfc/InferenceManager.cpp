#include "pch.h"
#include "InferenceManager.h"
#include "Logger.h"
#include <chrono>
#include <thread>
#include <sstream>
#include <Windows.h>
#include <cstring>

void InferenceManager::start(
    FrameQueue* inputQueue,
    FrameQueue* outputQueue,
    PlayMode playMode,
    vsHandle detector,
    CWnd* parent,
    UINT msgID
) {
    // Stop any existing thread first
    if (th_.joinable()) {
        stopFlag_ = true;
        if (inputQueue_)
            inputQueue_->shutdown();
        th_.join();
    }
    
    stopFlag_ = false;
    playMode_ = playMode;
    inputQueue_ = inputQueue;
    outputQueue_ = outputQueue;
    detector_ = detector;
    parent_ = parent;
    msgID_ = msgID;

    LOG_INFO_STREAM("[InferenceManager] Starting thread, detector: " << detector_ 
        << ", mode: " << (playMode_ == PlayMode::Continuous ? "Continuous" : "Timed"));
    
    th_ = std::thread(&InferenceManager::loop, this);
    
    LOG_INFO("[InferenceManager] Thread started successfully");
}

void InferenceManager::stop() 
{
    stopFlag_ = true;
    // Wake up the queue if it's waiting
    if (inputQueue_)
        inputQueue_->shutdown();
    if (th_.joinable())
        th_.join();
}

void InferenceManager::loop() 
{
    try {
        LOG_INFO_STREAM("[InferenceManager] Loop started, mode: " 
            << (playMode_ == PlayMode::Continuous ? "Continuous" : "Timed"));
        LOG_INFO_STREAM("[InferenceManager] Detector handle: " << detector_);
        LOG_INFO_STREAM("[InferenceManager] Input queue: " << inputQueue_);
        
        int processedCount = 0;
        while (!stopFlag_) {
            FrameInfo frame;
            bool gotFrame = false;
        
            // PlayMode::Continuous = drop old frames, process latest (like old Realtime)
            // PlayMode::Timed = process all frames in sequence (like old FullSequence)
            if (playMode_ == PlayMode::Continuous) {
                // Drain queue, take only latest (drop older frames)
                FrameInfo temp;
                int drained = 0;
                while (inputQueue_->try_pop(temp)) {
                    frame = std::move(temp);
                    gotFrame = true;
                    drained++;
                }
                // Debug: log if we drained many frames
                if (drained > 10) {
                    LOG_DEBUG_STREAM("[InferenceManager] Drained " << drained 
                        << " frames, processing latest (frame " << frame.frameIndex << ")");
                }
                // If no frame available, sleep briefly to avoid tight loop
                if (!gotFrame) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
            }
            else {
                // Timed mode: Try to pop with short timeout, skip frames if falling behind
                // If queue has many frames, skip old ones to catch up
                gotFrame = inputQueue_->pop_timeout(frame, std::chrono::milliseconds(100));
                
                // If we got a frame but queue is still large, we're falling behind
                // Skip to more recent frames to catch up
                if (gotFrame && inputQueue_->size() > 10) {
                    FrameInfo temp;
                    int skipped = 0;
                    // Skip frames until we're closer to real-time
                    while (inputQueue_->try_pop(temp)) {
                        frame = std::move(temp);
                        skipped++;
                        if (inputQueue_->size() <= 5) break; // Stop skipping when queue is reasonable
                    }
                    if (skipped > 0) {
                        LOG_DEBUG_STREAM("[InferenceManager] Skipped " << skipped 
                            << " frames to catch up, now processing frame " << frame.frameIndex);
                    }
                }
            }

            // Check stop flag after pop (queue shutdown may have woken us)
            if (stopFlag_)
                break;

            if (!gotFrame || frame.frame.empty()) {
                if (!gotFrame) {
                    // Only log occasionally to avoid spam
                    static int noFrameCount = 0;
                    if (++noFrameCount % 100 == 0) {
                        LOG_DEBUG_STREAM("[InferenceManager] No frame obtained (count: " << noFrameCount << ")");
                    }
                }
                continue;
            }
            
            processedCount++;
            if (processedCount % 10 == 0) {
                LOG_DEBUG_STREAM("[InferenceManager] Processing frame " << frame.frameIndex 
                    << " (total processed: " << processedCount << ")");
            }

            // Run detection (synchronously)
            if (!detector_ || frame.frame.empty()) {
                LOG_ERROR("[InferenceManager] Invalid YOLO handle or empty frame!");
                continue; // Continue loop instead of exiting
            }

            const unsigned char* imgData = frame.frame.data;
            int width = frame.frame.cols;
            int height = frame.frame.rows;
            int channels = frame.frame.channels();

            Detection* detections = nullptr;
            int count = 0;

            auto t0 = std::chrono::steady_clock::now();
            vsCode result = vsDetectObjects(detector_, imgData, width, height, channels, &detections, &count);
            auto t1 = std::chrono::steady_clock::now();
            auto detectionTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

            // Always send result, even if count == 0, so UI knows frame was processed
            if (result == VS_SUCCESS && parent_ && msgID_ != 0) {
                // Allocate detections array (even if empty, we need to send the result)
                Detection* detectionsToSend = nullptr;
                int countToSend = 0;
                
                if (detections != nullptr && count > 0) {
                    // Copy detections
                    detectionsToSend = new Detection[count];
                    std::memcpy(detectionsToSend, detections, count * sizeof(Detection));
                    countToSend = count;
                }
                
                // Notify parent window with frame index (always, even if no detections)
                struct DetectionResult {
                    Detection* detections;
                    int count;
                    int64_t frameIndex;
                    PlayMode playMode;
                };
                DetectionResult* resultPtr = new DetectionResult{detectionsToSend, countToSend, frame.frameIndex, playMode_};
                WPARAM wParam = (countToSend << 3) | 1;
                parent_->PostMessage(msgID_, wParam, reinterpret_cast<LPARAM>(resultPtr));
                
                // Debug output
                if (countToSend > 0 || processedCount % 30 == 0) {
                    LOG_DEBUG_STREAM("[InferenceManager] Frame " << frame.frameIndex 
                        << " processed in " << detectionTime << "ms, detections: " << countToSend);
                }
            }
            else {
                LOG_ERROR_STREAM("[InferenceManager] Detection failed for frame " << frame.frameIndex 
                          << ", error code: " << result << ", time: " << detectionTime << "ms");
            }

            // Push to output queue (non-blocking, drop if full)
            outputQueue_->push(frame, true);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR_STREAM("[InferenceManager] Exception in loop: " << e.what());
    }
    catch (...) {
        LOG_ERROR("[InferenceManager] Unknown exception in loop!");
    }
    LOG_INFO("[InferenceManager] Loop ended");
}
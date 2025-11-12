#pragma once

#include "FrameProc.h"
#include "SynopsisEngine.h"

enum class InferenceMode {
    Realtime,     // Only process latest frame (drop older)
    FullSequence  // Process every frame
};

class InferenceManager {
public:
    void start(
        FrameQueue* inputQueue,
        FrameQueue* outputQueue,
        InferenceMode mode,
        vsHandle detector
    );

    void stop();
private:
    void loop();
    std::thread th_;
    std::atomic<bool> stopFlag_{ false };
    InferenceMode mode_;
    FrameQueue* inputQueue_ = nullptr;
    FrameQueue* outputQueue_ = nullptr;
    vsHandle detector_ = nullptr;
};


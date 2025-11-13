#pragma once

#include "FrameProc.h"
#include "SynopsisEngine.h"
#include "VideoPlayer.h"

class InferenceManager {
public:
    void start(
        FrameQueue* inputQueue,
        FrameQueue* outputQueue,
        PlayMode playMode,
        vsHandle detector,
        CWnd* parent,
        UINT msgID
    );

    void stop();
private:
    void loop();
    std::thread th_;
    std::atomic<bool> stopFlag_{ false };
    PlayMode playMode_;
    FrameQueue* inputQueue_ = nullptr;
    FrameQueue* outputQueue_ = nullptr;
    vsHandle detector_ = nullptr;
    CWnd* parent_ = nullptr;
    UINT msgID_ = 0;
};


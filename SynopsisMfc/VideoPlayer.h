#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <opencv2/opencv.hpp>

// Unified mode for video playback, frame processing, and detection display
enum class PlayMode {
    Timed,       // Sync with playback: obey FPS, process all frames, display synchronized detections
    Continuous   // As fast as possible: no delay, drop old frames, display detections immediately
};


class VideoPlayer
{
public:
    using FrameCallback = std::function<void(const cv::Mat& frame, int64_t frameIdx, int64_t total, double fps)>;

    enum class State { Stopped, Paused, Playing };

    bool   Open(const TCHAR* path);
    void   Close();                 // stop + release
    bool   Play();
    void   Pause();
    void   Stop();                  // stop + seek to 0 (if possible)

    // Single-step (stays Paused afterwards)
    bool   NextFrame();             // +1 frame
    bool   PrevFrame();             // -1 frame (seek back one, read one)

    // Seek (absolute frame index)
    bool   SeekFrame(int64_t frameIndex);

    // Info
    State  GetState() const { return state_.load(); }
    int64_t CurrentFrame() const { return curFrame_; }
    int64_t FrameCount()  const { return totalFrames_; }
    double  FPS()         const { return fps_; }

    // UI callback: called on worker thread � post to UI thread in your handler
    void   SetCallback(FrameCallback cb) { on_frame_ = std::move(cb); }

    ~VideoPlayer() { Close(); }

    void SetPlayMode(PlayMode mode) {
        playMode_ = mode;
    }

    CString GetTotalTimeStr() const {
		return totalTimeStr_;
	}
    CString GetFrameTimeStr() const {
		return frameTimeStr_;
	}
    CString GetVideoSummary() const {
        return videoSummary_;
	}
private:
    // worker
    void   runLoop();
    bool   grabAndDispatch(); // read current (or next) frame and call callback
    CString FrameToTimestamp(int64_t frameIndex, double fps);
    void   extractVideoSummary();
private:
    cv::VideoCapture     cap_;
    std::thread          th_;
    std::atomic<bool>    alive_{ false };
    std::atomic<State>   state_{ State::Stopped };

    // stream props
    int64_t              totalFrames_ = -1;
    double               fps_ = 0.0;

    // position tracking
    std::atomic<int64_t> curFrame_{ 0 };

    // sync
    mutable std::mutex   mtx_;
    std::condition_variable cv_;
    FrameCallback        on_frame_;

    PlayMode            playMode_ = PlayMode::Timed;
    CString             videoPath_;
    CString             totalTimeStr_;
    CString			    frameTimeStr_;
    CString             videoSummary_;
};

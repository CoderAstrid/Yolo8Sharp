#include "pch.h"
#include "VideoPlayer.h"
#include <string>
#include <chrono>
#include <filesystem>
#include "Logger.h"

static std::string TCHAR_to_utf8(const TCHAR* path)
{
#ifdef UNICODE
    if (path == nullptr) 
        return std::string();

    // Calculate required buffer size (UTF-8 encoding)
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (utf8_size <= 0)
        return std::string();

    // Allocate buffer correctly (includes null terminator)
    std::vector<char> utf8_str(utf8_size);

    // Convert wchar_t (TCHAR in UNICODE) to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_str.data(), utf8_size, NULL, NULL);

    // Return std::string without the trailing null
    return std::string(utf8_str.data());
#else
    // ANSI mode: simple assignment
    return (path != nullptr) ? std::string(path) : std::string();
#endif
}

bool VideoPlayer::Open(const TCHAR* path)
{
    if (state_ != State::Stopped)
        Close(); // ensure clean start

    const std::string p = TCHAR_to_utf8(path);
    if (p.empty())
        return false;

    LOG_INFO_STREAM("[VideoPlayer] Opening video: " << p);    
    
    if (!std::filesystem::exists(p))
        LOG_ERROR("[VideoPlayer] File not found.");

    bool ok = cap_.open(p);
    if (!ok) {
        std::string info = cv::getBuildInformation();
        LOG_ERROR_STREAM("[VideoPlayer] Failed to open video. OpenCV Build Info:\n" << info);
        return false;
    }

    videoPath_ = path;
    totalFrames_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_FRAME_COUNT)); // can be -1 for some codecs
    fps_ = cap_.get(cv::CAP_PROP_FPS);
    if (fps_ <= 1e-3)
        fps_ = 30.0; // fallback

    curFrame_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    totalTimeStr_ = FrameToTimestamp(totalFrames_, fps_);
    frameTimeStr_ = FrameToTimestamp(0, fps_);
    extractVideoSummary();

    grabAndDispatch(); // deliver first frame

    state_ = State::Paused;
    alive_ = true;
    th_ = std::thread(&VideoPlayer::runLoop, this);

    return true;
}

void VideoPlayer::extractVideoSummary()
{   
    if(!cap_.isOpened())
        return;
    int64_t frameCount = static_cast<int64_t>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    double fps = cap_.get(cv::CAP_PROP_FPS);
    double duration = (fps > 0) ? frameCount / fps : 0.0;
    int width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fourcc = static_cast<int>(cap_.get(cv::CAP_PROP_FOURCC));

    char codec[] = {
        static_cast<char>(fourcc & 0xFF),
        static_cast<char>((fourcc >> 8) & 0xFF),
        static_cast<char>((fourcc >> 16) & 0xFF),
        static_cast<char>((fourcc >> 24) & 0xFF),
        0
    };

    CString s;
    s.Format(_T(
        "Video Summary:\n"
        "  Path: %s\n"
        "  Size: %d x %d\n"
        "  FPS: %.2f\n"
        "  Frames: %lld\n"
        "  Duration: %.2f seconds\n"
        "  Codec: %s\n"),
        videoPath_, width, height, fps, frameCount, duration, CString(codec));
    videoSummary_ = s;
}

void VideoPlayer::Close()
{
    alive_ = false;
    cv_.notify_all();
    if (th_.joinable()) 
        th_.join();
    if (cap_.isOpened())
        cap_.release();
    state_ = State::Stopped;
    curFrame_ = 0;
    totalFrames_ = -1;
    fps_ = 0.0;

    videoPath_.Empty();
    totalTimeStr_.Empty();
    frameTimeStr_.Empty();
    videoSummary_.Empty();
}

bool VideoPlayer::Play()
{
    if (!cap_.isOpened())
        return false;
    state_ = State::Playing;
    cv_.notify_all();
    return true;
}

void VideoPlayer::Pause()
{
    state_ = State::Paused;
    cv_.notify_all(); // Wake up thread so it sees the state change
}

void VideoPlayer::Stop()
{
    // Set state to Stopped and notify the thread
    state_ = State::Stopped;
    cv_.notify_all(); // Wake up the thread so it can check the state
    
    // Seek to beginning if video is opened
    if (cap_.isOpened()) {
        // Use mutex to protect cap_ access from concurrent runLoop() calls
        std::lock_guard<std::mutex> lk(mtx_);
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
        curFrame_ = 0;
        frameTimeStr_ = FrameToTimestamp(0, fps_);
        
        // Deliver the first frame (optional, but useful for UI)
        cv::Mat frame;
        if (cap_.read(frame) && !frame.empty()) {
            curFrame_ = 0;
            if (on_frame_) {
                on_frame_(frame, 0, totalFrames_, fps_);
            }
        }
    }
}

bool VideoPlayer::SeekFrame(int64_t frameIndex)
{
    if (!cap_.isOpened() || frameIndex < 0) 
        return false;
    
    // Pause playback during seek
    state_ = State::Paused;
    cv_.notify_all();
    
    std::unique_lock<std::mutex> lk(mtx_);
    
    // Set position (OpenCV expects double)
    if (!cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frameIndex)))
        return false;
    
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty())
        return false;
    
    int64_t posNext = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    curFrame_ = posNext - 1;
    frameTimeStr_ = FrameToTimestamp(curFrame_, fps_);
    
    lk.unlock();
    if (on_frame_)
        on_frame_(frame, curFrame_.load(), totalFrames_, fps_);
    return true;
}

bool VideoPlayer::NextFrame()
{
    if (!cap_.isOpened())
        return false;
    state_ = State::Paused;
    cv_.notify_all(); // Wake up thread so it sees the state change
    
    // We're already at current frame index; reading advances by 1
    std::unique_lock<std::mutex> lk(mtx_);
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty())
        return false;
    
    int64_t posNext = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    curFrame_ = posNext - 1;
    frameTimeStr_ = FrameToTimestamp(curFrame_, fps_);
    
    lk.unlock();
    if (on_frame_)
        on_frame_(frame, curFrame_.load(), totalFrames_, fps_);
    return true;
}

bool VideoPlayer::PrevFrame()
{
    if (!cap_.isOpened()) 
        return false;
    state_ = State::Paused;
    cv_.notify_all(); // Wake up thread so it sees the state change

    std::unique_lock<std::mutex> lk(mtx_);
    
    int64_t idx = curFrame_.load();
    // After a successful read, OpenCV's POS_FRAMES points to "next".
    // So to show previous frame, jump to (idx - 2), then read one.
    int64_t target = idx - 2;
    if (target < 0) 
        target = 0;

    if (!cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(target)))
        return false;

    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty())
        return false;
    
    int64_t posNext = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    curFrame_ = posNext - 1;
    frameTimeStr_ = FrameToTimestamp(curFrame_, fps_);
    
    lk.unlock();
    if (on_frame_)
        on_frame_(frame, curFrame_.load(), totalFrames_, fps_);
    return true;
}

void VideoPlayer::runLoop()
{
    using namespace std::chrono;
    auto frameDur = duration<double>(1.0 / std::max(1.0, fps_));

    while (alive_)
    {
        State currentState = state_.load();
        
        // If stopped, wait until state changes (but don't process frames)
        if (currentState == State::Stopped) {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { 
                return !alive_ || state_ != State::Stopped; 
            });
            continue; // Check state again after waking
        }
        
        // If paused, wait until playing or stopped
        if (currentState == State::Paused) {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { 
                return !alive_ || state_ == State::Playing || state_ == State::Stopped; 
            });
            continue; // Check state again after waking
        }

        // Only process frames when Playing
        if (currentState != State::Playing) {
            continue;
        }

        auto t0 = steady_clock::now();
        if (!grabAndDispatch()) {
            // End of stream reached
            LOG_INFO("[VideoPlayer] End of stream reached, pausing playback");
            state_ = State::Paused; // end of stream
            continue;
        }
        
        // Mode logic
        if (playMode_ == PlayMode::Timed) {
            auto t1 = steady_clock::now();
            auto elapsed = t1 - t0;

            if (elapsed < frameDur)
                std::this_thread::sleep_for(frameDur - elapsed);
        }
        // else Continuous: do not sleep
    }
}

bool VideoPlayer::grabAndDispatch()
{
    // Protect cap_ access with mutex to prevent concurrent access from Stop()
    std::unique_lock<std::mutex> lk(mtx_);
    
    // Check if we should still be processing (state might have changed)
    if (state_ != State::Playing || !cap_.isOpened()) {
        return false;
    }
    
    cv::Mat frame;
    if (!cap_.read(frame) || frame.empty()) 
        return false;

    // POS_FRAMES is now "next index", so the current frame index is:
    int64_t posNext = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    curFrame_ = posNext - 1; // best-effort tracking

    frameTimeStr_ = FrameToTimestamp(curFrame_, fps_);

    // Release lock before calling callback (callback might be slow)
    lk.unlock();
    
    if (on_frame_) 
        on_frame_(frame, curFrame_.load(), totalFrames_, fps_);
    return true;
}

CString VideoPlayer::FrameToTimestamp(int64_t frameIndex, double fps)
{
    if (fps <= 1e-3)
        return _T("00:00:00"); // avoid div by 0

    double totalSec = frameIndex / fps;

    int hours = static_cast<int>(totalSec / 3600);
    int minutes = static_cast<int>((totalSec - hours * 3600) / 60);
    int seconds = static_cast<int>(totalSec) % 60;
    // int millis = static_cast<int>((totalSec - static_cast<int>(totalSec)) * 1000);

    CString res;
    res.Format(_T("%02d:%02d:%02d"), hours, minutes, seconds);
    return res;
}
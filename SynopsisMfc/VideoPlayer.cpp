#include "pch.h"
#include "VideoPlayer.h"
#include <string>
#include <chrono>
#include <filesystem>

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
    return (tstr != nullptr) ? std::string(tstr) : std::string();
#endif
}

bool VideoPlayer::Open(const TCHAR* path)
{
    Close();

    const std::string p = TCHAR_to_utf8(path);
    if (p.empty())
        return false;

    OutputDebugStringA(("Opening video: " + p + "\n").c_str());
    if (!std::filesystem::exists(p))
        OutputDebugStringA("File not found!\n");

    bool ok = cap_.open(p);
    if (!ok) {
        std::string info = cv::getBuildInformation();
        OutputDebugStringA(("OpenCV Build Info:\n" + info + "\n").c_str());
        return false;
    }

    totalFrames_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_FRAME_COUNT)); // can be -1 for some codecs
    fps_ = cap_.get(cv::CAP_PROP_FPS);
    if (fps_ <= 1e-3)
        fps_ = 30.0; // fallback

    curFrame_ = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));

    grabAndDispatch(); // deliver first frame

    state_ = State::Paused;
    alive_ = true;
    th_ = std::thread(&VideoPlayer::runLoop, this);
    return true;
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
}

void VideoPlayer::Stop()
{
    state_ = State::Stopped;
    if (cap_.isOpened()) {
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
        curFrame_ = 0;
    }
    // deliver the first frame (optional)
    grabAndDispatch();
}

bool VideoPlayer::SeekFrame(int64_t frameIndex)
{
    if (!cap_.isOpened() || frameIndex < 0) 
        return false;
    // Set position (OpenCV expects double)
    if (!cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(frameIndex)))
        return false;
    curFrame_ = frameIndex;
    return grabAndDispatch();
}

bool VideoPlayer::NextFrame()
{
    if (!cap_.isOpened())
        return false;
    state_ = State::Paused;
    // We’re already at current frame index; reading advances by 1
    return grabAndDispatch();
}

bool VideoPlayer::PrevFrame()
{
    if (!cap_.isOpened()) 
        return false;
    state_ = State::Paused;

    int64_t idx = curFrame_.load();
    // After a successful read, OpenCV’s POS_FRAMES points to “next”.
    // So to show previous frame, jump to (idx - 2), then read one.
    int64_t target = idx - 2;
    if (target < 0) 
        target = 0;

    if (!cap_.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(target)))
        return false;

    curFrame_ = target;
    return grabAndDispatch();
}

void VideoPlayer::runLoop()
{
    using namespace std::chrono;
    auto frameDur = duration<double>(1.0 / std::max(1.0, fps_));

    while (alive_)
    {
        if (state_ != State::Playing) {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { 
                return !alive_ || state_ == State::Playing; 
            });
            continue;
        }

        auto t0 = steady_clock::now();
        if (!grabAndDispatch()) {
            state_ = State::Paused; // end of stream
            continue;
        }
        auto t1 = steady_clock::now();
        auto elapsed = t1 - t0;

        if (elapsed < frameDur)
            std::this_thread::sleep_for(frameDur - elapsed);
    }
}

bool VideoPlayer::grabAndDispatch()
{
    cv::Mat frame;
    if (!cap_.read(frame)) 
        return false;

    // POS_FRAMES is now “next index”, so the current frame index is:
    int64_t posNext = static_cast<int64_t>(cap_.get(cv::CAP_PROP_POS_FRAMES));
    curFrame_ = posNext - 1; // best-effort tracking

    if (on_frame_) 
        on_frame_(frame, curFrame_.load(), totalFrames_, fps_);
    return true;
}

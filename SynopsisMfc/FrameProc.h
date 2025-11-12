#ifndef __YOLOTESTAPP_FRAMEPROC_H__
#define __YOLOTESTAPP_FRAMEPROC_H__

#include <opencv2/opencv.hpp>
#include <cstdint>
#include <queue>
#include <mutex>
#include <condition_variable>

struct FrameInfo {
    cv::Mat frame;
    int64_t frameIndex = -1;
    double timestamp = 0.0; // in seconds

    FrameInfo() = default;
    FrameInfo(const cv::Mat& img, int64_t idx, double time)
        : frame(img.clone()), frameIndex(idx), timestamp(time) {}
};

template <typename T>
class SafeQueue {
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cv_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !queue_.empty(); });
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

typedef SafeQueue<FrameInfo> FrameQueue;

#endif//__YOLOTESTAPP_FRAMEPROC_H__
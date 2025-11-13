#ifndef __YOLOTESTAPP_FRAMEPROC_H__
#define __YOLOTESTAPP_FRAMEPROC_H__

#include <opencv2/opencv.hpp>
#include <cstdint>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

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
    SafeQueue(size_t maxSize = 0) : shutdown_(false), maxSize_(maxSize) {}
    
    // Push with option to drop oldest if queue is full
    bool push(const T& item, bool dropOldestIfFull = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return false; // Don't accept new items after shutdown
        
        // If queue is full and we should drop oldest
        if (maxSize_ > 0 && queue_.size() >= maxSize_) {
            if (dropOldestIfFull) {
                queue_.pop(); // Remove oldest
            } else {
                return false; // Queue full, cannot add
            }
        }
        
        queue_.push(item);
        cv_.notify_one();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !queue_.empty() || shutdown_; });
        if (shutdown_ && queue_.empty())
            return false; // Shutdown and no items left
        if (queue_.empty())
            return false; // Shouldn't happen, but safety check
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty() || shutdown_) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // Pop with timeout - returns false if timeout expires or queue is empty
    template<typename Rep, typename Period>
    bool pop_timeout(T& item, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, timeout, [&] { return !queue_.empty() || shutdown_; })) {
            if (shutdown_ && queue_.empty())
                return false;
            if (queue_.empty())
                return false;
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false; // Timeout
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cv_.notify_all(); // Wake all waiting threads
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = false;
        // Clear any remaining items
        std::queue<T> empty;
        std::swap(queue_, empty);
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

    bool isFull() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return maxSize_ > 0 && queue_.size() >= maxSize_;
    }

    size_t maxSize() const { return maxSize_; }
    void setMaxSize(size_t maxSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxSize_ = maxSize;
        // Optionally trim queue if new max is smaller
        while (maxSize_ > 0 && queue_.size() > maxSize_) {
            queue_.pop();
        }
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_;
    size_t maxSize_; // 0 means unbounded
};

typedef SafeQueue<FrameInfo> FrameQueue;

#endif//__YOLOTESTAPP_FRAMEPROC_H__
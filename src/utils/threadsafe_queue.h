// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_UTILS_THREADSAFEQUEUE_H_
#define SQLIZATOR_UTILS_THREADSAFEQUEUE_H_

#include<condition_variable>
#include<memory>
#include<mutex>
#include<queue>


namespace utils {
template<typename T>
class ThreadsafeQueue {
 private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable queue_var_;

 public:
    ThreadsafeQueue() {  }
    ThreadsafeQueue(ThreadsafeQueue const& other) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = other.queue_;
    }

    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        queue_var_.notify_one();
    }

    void push(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::forward<T>(value));
        queue_var_.notify_one();
    }

    void wait_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_var_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
    }

    std::shared_ptr<T> wait_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_var_.wait(lock, [this] { return !queue_.empty(); });
        std::shared_ptr<T> res(std::make_shared<T>(queue_.front()));
        queue_.pop();
        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if(queue_.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(queue_.front()));
        queue_.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};
} // namespace utils
#endif // SQLIZATOR_UTILS_THREADSAFEQUEUE_H_

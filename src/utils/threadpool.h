// Copyright 2015, Outernet Inc.
// Some rights reserved.
// This software is free software licensed under the terms of GPLv3. See COPYING
// file that comes with the source code, or http://www.gnu.org/licenses/gpl.txt.
#ifndef SQLIZATOR_UTILS_THREADPOOL_H_
#define SQLIZATOR_UTILS_THREADPOOL_H_

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


namespace utils {
class ThreadPool {
 private:
    std::vector< std::thread > workers_;
    std::queue< std::function<void()> > task_queue_;

    std::mutex queue_mutex_;
    std::condition_variable wait_var_;

    size_t size_;
    volatile bool stop_;

    void worker_run();

 public:
    explicit ThreadPool(size_t size = 4);
    template<class FunctionType> void submit(FunctionType f);
    void join();
    ~ThreadPool();
};

inline ThreadPool::ThreadPool(size_t size) : size_(size), stop_(false) {
    for(size_t i = 0; i < size_; ++i) {
        workers_.push_back(std::thread (&ThreadPool::worker_run, this));
    }
}

template<class FunctionType>
void ThreadPool::submit(FunctionType f) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if(stop_) {
            throw std::runtime_error("submit called on stopped ThreadPool");
        }
        task_queue_.push(std::function<void()>(f));
    }
    wait_var_.notify_one();
}

inline void ThreadPool::join() {
    for(std::thread &worker: workers_) {
        worker.join();
    }
}

inline void ThreadPool::worker_run() {
    while(true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            wait_var_.wait(lock,
                    [this] { return this->stop_ || !this->task_queue_.empty(); });
            if(stop_ && task_queue_.empty()) {
                return;
            }
            task = std::move(task_queue_.front());
            task_queue_.pop();
        }
        task();
    }
}

inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    wait_var_.notify_all();
    join();
}

}  // namespace utils
#endif  // SQLIZATOR_UTILS_THREADPOOL_H_

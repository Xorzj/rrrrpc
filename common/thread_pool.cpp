#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) 
    : stop_(false), active_threads_(0), pending_tasks_(0) {
    
    std::cout << "[ThreadPool] Creating thread pool with " << num_threads << " threads" << std::endl;
    
    workers_.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i] {
            std::cout << "[ThreadPool] Worker thread " << i << " started" << std::endl;
            
            for (;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    
                    // 等待任务或停止信号
                    this->condition_.wait(lock, [this] {
                        return this->stop_ || !this->tasks_.empty();
                    });
                    
                    if (this->stop_ && this->tasks_.empty()) {
                        std::cout << "[ThreadPool] Worker thread " << i << " stopping" << std::endl;
                        return;
                    }
                    
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                    --pending_tasks_;
                }
                
                // 执行任务
                ++active_threads_;
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "[ThreadPool] Task execution failed: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[ThreadPool] Task execution failed with unknown exception" << std::endl;
                }
                --active_threads_;
                
                // 通知等待完成的线程
                if (active_threads_ == 0 && pending_tasks_ == 0) {
                    finished_condition_.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::stop() {
    if (stop_) return;
    
    std::cout << "[ThreadPool] Stopping thread pool..." << std::endl;
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "[ThreadPool] Thread pool stopped" << std::endl;
}

size_t ThreadPool::getQueueSize() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::waitForCompletion() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    finished_condition_.wait(lock, [this] {
        return tasks_.empty() && active_threads_ == 0;
    });
}
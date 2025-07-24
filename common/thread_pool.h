#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>

class ThreadPool {
public:
    // 构造函数：创建指定数量的工作线程
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    
    // 析构函数：停止所有线程并等待完成
    ~ThreadPool();
    
    // 禁用拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    // 提交任务到线程池
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // 获取统计信息
    size_t getActiveThreads() const { return active_threads_.load(); }
    size_t getQueueSize() const;
    size_t getTotalThreads() const { return workers_.size(); }
    
    // 停止线程池
    void stop();
    
    // 等待所有任务完成
    void waitForCompletion();
    
    // 检查是否正在运行
    bool isRunning() const { return !stop_.load(); }

private:
    // 工作线程
    std::vector<std::thread> workers_;
    
    // 任务队列
    std::queue<std::function<void()>> tasks_;
    
    // 同步原语
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable finished_condition_;
    
    // 状态标志
    std::atomic<bool> stop_;
    std::atomic<size_t> active_threads_;
    std::atomic<size_t> pending_tasks_;
};

// 模板方法的实现必须在头文件中
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    // 创建任务包装器
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 不允许在停止的线程池中添加任务
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        // 添加任务到队列
        tasks_.emplace([task](){ (*task)(); });
        ++pending_tasks_;
    }
    
    // 通知工作线程
    condition_.notify_one();
    return res;
}

#endif // THREAD_POOL_H
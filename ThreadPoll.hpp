//
// Created by LLH on 2022/1/30.
//
#ifndef SERVER_THREADPOLL_HPP
#define SERVER_THREADPOLL_HPP
#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>
#include<memory>
#include "boost/lockfree/queue.hpp"
class ThreadPool {
private:
    const uint16_t THREADPOOL_MAX_NUM=16;
    using Task = std::function<void()>;
    std::vector<std::thread> MyPool;//线程池
    std::queue<Task> MyTaskQueue;//任务队列
//    boost::lockfree::queue<Task*> FreeTakQueue;//尝试使用无锁队列
    std::mutex MyLock;//互斥锁
    std::condition_variable ConLock;//条件锁
    std::atomic<bool> Running{true};//运行状态
    std::atomic<uint16_t> NoTaskNum{0};//空闲线程
    explicit ThreadPool(uint16_t size = 2){
        MyPool.reserve(THREADPOOL_MAX_NUM);
        this->addThread(size);
    };
public:
    ThreadPool operator=(ThreadPool& rhs)=delete;
    ThreadPool(ThreadPool& rhs)=delete;
    static ThreadPool& getThreadPool(){
        static ThreadPool ans;
        return ans;
    }
    void addThread(uint16_t size){
        for (; MyPool.size() < THREADPOOL_MAX_NUM && size > 0; --size) {
            MyPool.emplace_back([this]() {
                while (Running) {
                    std::unique_lock<std::mutex> templock(MyLock);
                    ConLock.wait(templock, [this]() {
                        return !Running || !MyTaskQueue.empty();
                    });
                    if (!Running && MyTaskQueue.empty()) {
                        return;
                    }
                    Task task(std::move(MyTaskQueue.front()));
                    MyTaskQueue.pop();
                    templock.unlock();
                    --NoTaskNum;
                    task();
                    ++NoTaskNum;
                }
            });
            ++NoTaskNum;
        }
    };
    template<typename F, typename...Args>
    auto commit(F &&f, Args &&... args) -> std::future<decltype(f(args...))> {
        using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型
        auto task = std::make_shared<std::packaged_task<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        ); // 把函数入口及参数,打包(绑定)
        std::future<RetType> future = task->get_future();
        {    // 添加任务到队列
//            std::lock_guard<std::mutex> lock{MyLock};//对当前块的语句加锁  lock_guard 是 mutex 的 stack 封装类，构造的时候 lock()，析构的时候 unlock()
            MyLock.lock();
            MyTaskQueue.emplace([task]() { // push(Task{...}) 放到队列后面
                (*task)();
            });
            MyLock.unlock();
        }

        if (NoTaskNum < 1 && MyPool.size() < THREADPOOL_MAX_NUM) {
            addThread(1);
        }
        ConLock.notify_one(); // 唤醒一个线程执行
        return future;
    }
    uint16_t FreeThreadNum(){
        return NoTaskNum;
    };
    uint32_t CountThreadNum(){
        return MyPool.size();
    };
    ~ThreadPool(){
        Running = false;
        ConLock.notify_all();
        for (std::thread &th: MyPool) {
            if (th.joinable()) {
                th.join();
            }
        }
    };
};
#endif //SERVER_THREADPOLL_HPP

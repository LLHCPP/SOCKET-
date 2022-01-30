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
#define  THREADPOOL_MAX_NUM 16
class ThreadPool {
private:
    using Task = std::function<void()>;
    std::vector<std::thread> MyPool;//线程池
    std::queue<Task> MyTaskQueue;//任务队列
    std::mutex MyLock;//互斥锁
    std::condition_variable ConLock;//条件锁
    std::atomic<bool> Running{true};//运行状态
    std::atomic<uint16_t> NoTaskNum{0};
    explicit ThreadPool(uint16_t size = 2){
        this->addThread(size);
    };
    ThreadPool operator=(ThreadPool& rhs)=delete;
    ThreadPool(ThreadPool& rhs)=delete;
public:
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
                    Task task(move(MyTaskQueue.front()));
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
        auto task = make_shared<std::packaged_task<RetType()>>(
                bind(forward<F>(f), forward<Args>(args)...)
        ); // 把函数入口及参数,打包(绑定)
        std::future<RetType> future = task->get_future();
        {    // 添加任务到队列
            std::lock_guard<std::mutex> lock{MyLock};//对当前块的语句加锁  lock_guard 是 mutex 的 stack 封装类，构造的时候 lock()，析构的时候 unlock()
            MyTaskQueue.emplace([task]() { // push(Task{...}) 放到队列后面
                (*task)();
            });
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

/**
 * @brief 生产者-消费者队列（互斥锁 + 条件变量版）
 *
 * ============================================================
 * 第 3 周 · 周一
 * 知识点: std::mutex, std::condition_variable, 优雅退出
 * ============================================================
 *
 * ---- 要求 ----
 * 实现一个线程安全的阻塞队列，支持:
 *  1. push(T value)        — 生产者放入数据，通知消费者
 *  2. T pop()              — 消费者阻塞等待，直到有数据或 stop()
 *  3. stop() + notify_all() — 优雅退出: 唤醒所有等待线程，pop() 抛异常或返回 false
 *
 * ---- 知识点: condition_variable 的陷阱 ----
 *
 *  虚唤醒 (spurious wakeup):
 *    condition_variable::wait 可能在没有被 notify 的情况下返回。
 *    因此必须使用 predicate 重载: wait(lock, [&]{ return !queue.empty() || stopped; })
 *    这等价于: while (!可以继续的条件) wait(lock);
 *
 *  notify_one vs notify_all:
 *    notify_one   — 只唤醒一个等待线程（高效，但可能唤醒的是消费者而非生产者）
 *    notify_all   — 唤醒所有线程（用于 stop() 场景）
 *    在有界队列中，生产者和消费者等待不同条件，必须用 notify_all。
 *
 *  unique_lock 的必要性:
 *    wait() 内部需要 unlock 锁，让其他线程能获取锁并修改条件。
 *    lock_guard 不提供 unlock()，所以无法用于 condition_variable。
 *
 * ---- 优雅退出模式 ----
 *
 *  void stop() {
 *      lock_guard<mutex> lock(mtx_);
 *      stopped_ = true;
 *      cv_.notify_all();  // 唤醒所有等待的消费者
 *  }
 *
 *  消费者检查 stopped_ 标志，如果已经停止则退出循环或抛异常。
 *
 * ---- 面试追问 ----
 *  - Q: pop() 返回 T 还是 bool? 各有什么优劣？
 *    A: T pop() 在停止时抛异常（RAII 不友好）；
 *       bool pop(T&) 通过返回值指示成功/停止，更安全。
 *       也可以返回 std::optional<T>。
 *
 *  - Q: 虚唤醒 (spurious wakeup) 为什么存在？
 *    A: 操作系统 pthread_cond_wait 的实现原因:
 *       信号从内核发送到用户态可能被中途拦截；
 *       多核 CPU 上的进程迁移也可能导致虚唤醒。
 *       这是 POSIX 标准允许的行为，不是 bug。
 *
 *  - Q: unique_lock 比 lock_guard 重吗？代价是什么？
 *    A: unique_lock 多了 bool owns_lock_ 成员和更多的成员函数，
 *       体积略大（多一个 bool），性能差异在多数场景可忽略。
 *       优先用 lock_guard，需要 unlock 时才用 unique_lock。
 */

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

// ============================================================
// TODO: 实现 ThreadSafeQueue
// ============================================================
// 模板参数: T — 队列元素类型
// 成员:
//   std::queue<T>     queue_;    // 内部队列
//   std::mutex        mtx_;      // 互斥锁
//   std::condition_variable cv_; // 条件变量
//   bool              stopped_{false};
//
// 接口:
//   void push(T value)        — 加锁 push，cv_.notify_one()
//   T pop()                   — wait 直到有数据或 stopped，抛异常或返回
//   bool try_pop(T& value)    — 非阻塞版，无数据立刻返回 false
//   void stop()               — stopped_ = true; cv_.notify_all()
//   bool empty() const        — 检查队列是否为空
//   size_t size() const       — 返回队列大小
// ============================================================
template<typename T>
class ThreadSafeQueue
{
private:
    std::queue<T>           queue_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    bool                    stopped_{false};

public:
    void push(const T& value)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(value);

        cv_.notify_one();
    }

    // 【BUG】pop() 在 stop() 后空队列上调用 queue_.front() → UB
    // 修复: wait 后检查 queue_.empty()，空则返回 nullopt
    // cv_.wait(uni_lock, [&] { return !queue_.empty() || stopped_; });
    // if (queue_.empty()) return std::nullopt;  // stopped, nothing to pop
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> uni_lock(mtx_);

        cv_.wait(uni_lock, [&] { return !queue_.empty() || stopped_; });
        // 如果 stopped_ == true 且队列为空，下行 front() 是 UB
        if (queue_.empty())
            return std::nullopt;

        T val = queue_.front();
        queue_.pop();

        return std::optional<T>(val);
    }

    // 【BUG】stop() 写入 stopped_ 未加锁，与 pop() 中的读取形成 data race
    // 修复: std::lock_guard<std::mutex> lock(mtx_); 后再设置 stopped_
    void stop()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stopped_ = true;
        cv_.notify_all();
    }

    // 【BUG】is_stop() 读取 stopped_ 无同步 → data race（与 stop() 中的写入竞争）
    // 修复: std::atomic<bool> stopped_; 或加锁读取
    bool is_stop() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return stopped_;
    }
    // 【BUG】empty() 读取 queue_ 内部状态无锁保护 → data race（与 push/pop 竞争）
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    // 【BUG】size() 读取 queue_ 内部状态无锁保护 → data race
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
};

using namespace std::chrono_literals;
// ============================================================
// TODO: main() — 启动 2 个生产者 + 2 个消费者，运行 1 秒后 stop()
// ============================================================
int main()
{
    std::cout << "day01: mutex + condition_variable producer-consumer" << std::endl;

    // TODO: 创建 ThreadSafeQueue<int>
    // TODO: 启动生产者线程（push 数据）
    // TODO: 启动消费者线程（pop 并打印）
    // TODO: sleep 1 秒后调用 stop()
    // TODO: join 所有线程

    ThreadSafeQueue<int> que;

    std::thread producer_thread([&] {
        int i = 0;
        while (!que.is_stop()) {
            que.push(i);
            std::cout << "producer_thread push val: " << i++ << "\n";
        }
    });

    // 【BUG】drain 循环中 que.empty() 无锁 + que.pop() 在停止后可能对空队列 front → UB
    // 应使用 try_pop() 非阻塞弹出，而非复用 condvar 版 pop()
    std::thread consumer_thread([&] {
        while (!que.is_stop()) {
            auto val = que.pop();
            if (val.has_value()) {
                std::cout << "consumer_thread pop val: " << val.value() << "\n";
            }
        }

        while (!que.empty()) {
            auto val = que.pop();
            // if (val.has_value()) {
            //     std::cout << "consumer_thread pop val: " << val.value() << "\n";
            // }
        }
        std::cout << "que empty\n";
    });
    std::thread consumer_thread1([&] {
        while (!que.is_stop()) {
            auto val = que.pop();
            if (val.has_value()) {
                std::cout << "consumer_thread pop val: " << val.value() << "\n";
            }
        }

        while (!que.empty()) {
            auto val = que.pop();
            // if (val.has_value()) {
            //     std::cout << "consumer_thread pop val: " << val.value() << "\n";
            // }
        }
        std::cout << "que empty\n";
    });
    std::this_thread::sleep_for(1s);
    que.stop();

    producer_thread.join();
    consumer_thread.join();
    consumer_thread1.join();

    return 0;
}

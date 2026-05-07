/**
 * @brief 多生产者 - 多消费者队列（MPMC）
 *
 * ============================================================
 * 第 3 周 · 周四
 * 知识点: 多线程同步, CAS, 竞争条件, 正确同步策略
 * ============================================================
 *
 * ---- 要求 ----
 * 实现一个多生产者-多消费者安全的阻塞队列:
 *  1. 多个生产者线程可以同时 push
 *  2. 多个消费者线程可以同时 pop
 *  3. 使用 std::mutex + std::condition_variable 保护
 *     或用原子操作 + 无锁方式
 *  4. 支持优雅退出 stop()
 *
 * ---- 设计决策 ----
 *
 *  方案 A: 单锁（一把锁保护全部）
 *    简单，但在高竞争下性能差。
 *    所有生产者和消费者抢同一把锁。
 *
 *  方案 B: 两把锁（生产锁 + 消费锁）
 *    队列内部可以用链表（而不是 queue），
 *    生产者只竞争"尾指针锁"，消费者只竞争"头指针锁"。
 *    这是高性能 MPMC 队列的基础。
 *
 *  方案 C: 无锁 MPMC
 *    基于 CAS 操作多个原子指针。
 *    需要解决 ABA 问题，非常复杂。不推荐初学手写。
 *
 * ---- 正确同步的关键 ----
 *
 *  通知策略:
 *    notify_one 够用吗? 不够！消费者 A 被唤醒，但可能另一个消费者 B 抢走了数据。
 *    所以: push 后应该 notify_one（只唤醒一个消费者即可尝试取数据）。
 *         stop() 时用 notify_all。
 *
 *  虚假唤醒防护:
 *    wait(lock, [&]{ return !queue.empty() || stopped; });
 *    这是必须的，即使是 MPMC 场景。
 *
 *  data race 的隐蔽形式:
 *    典型错误: if (!queue.empty()) { value = queue.front(); queue.pop(); }
 *    两个消费者同时通过 !empty() 检查 → 都去 front/pop → 崩溃或数据竞争。
 *    必须在锁的保护下检查、读取、弹出三步连续完成。
 *
 * ---- 面试追问 ----
 *  - Q: 两个消费者同时等待数据，生产者 push 后 notify_one 会怎样？
 *    A: notify_one 唤醒一个消费者。被唤醒的消费者获取锁，检查条件，取走数据。
 *       另一个消费者保持等待。如果被唤醒的消费者没有取走数据（虚假唤醒），
 *       predicate 检查会失败，重新 wait。
 *
 *  - Q: 如何测量 MPMC 的性能瓶颈？
 *    A: 锁竞争是主要瓶颈。用 perf 查看 spin 时间，或用 ThreadSanitizer
 *       检测数据竞争。高竞争场景下，两把锁方案比单锁有显著优势。
 *
 *  - Q: 如果消费者处理数据太慢，生产者快，有什么影响？
 *    A: 队列膨胀 → 内存占用增加。如果有界队列则生产者阻塞（背压 backpressure）。
 *       这是反应式系统（Reactive Streams）的核心机制。
 */

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

// ============================================================
// TODO: 实现 MPMCQueue (单锁版本)
// ============================================================
// 模板参数: T — 元素类型
// 成员:
//   std::queue<T>     queue_;
//   std::mutex        mtx_;
//   std::condition_variable cv_;
//   bool              stopped_{false};
//
// 接口: 与 day01 相同，但注意:
//   多个生产者调用 push 是安全的（mutex 保护）
//   多个消费者调用 pop 是安全的（mutex 保护）
//   需要处理"多消费者被唤醒后数据被抢走"的情况
//   （predicate 中的 !queue.empty() 天然解决此问题）
// ============================================================
template<typename T>
class MPMCQueue
{
private:
    std::queue<T>           queue_;
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    bool                    stopped_{false};

public:
    void push(const T& val)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(val);
        }
        // 【设计】notify_one 在锁外调用：被唤醒的消费者不会立即阻塞在锁上，
        // 减少一次不必要的锁竞争（生产者已释放锁）。
        // 若在锁内 notify_one，消费者被唤醒后要先等锁 → 额外上下文切换开销。
        cv_.notify_one();
    }

    // NOTE: 缺少 push(T&&) 移动重载。对于 string 等类型，临时对象会先拷贝再入队。

    // 【设计】返回 std::optional<T> 而非 T:
    //   stop() 后消费者需要区分"正常取到数据"和"队列已停止且为空"。
    //   若返回 T，无法表达空状态（抛异常或返回默认值都不合适）。
    //   std::nullopt 是优雅退出的通信协议基础。
    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(mtx_);

        //  不为空或停止标志为true则唤醒
        cv_.wait(lock, [&] { return !queue_.empty() || stopped_; });

        // 【关键】stop() 可能唤醒 cv_.wait()，但队列可能已被其他消费者取空。
        // 此时 stopped_ = true 且 queue_.empty() → 返回 nullopt 让调用方退出。
        // 没有这个检查: stop() 后对空队列调用 front() → UB。
        if (queue_.empty())
            return std::nullopt;

        T val = std::move(queue_.front());
        queue_.pop();

        return std::optional<T>(std::move(val));
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stopped_ = true;
        cv_.notify_all(); // 【必须】notify_all 唤醒所有在 cv_.wait() 中阻塞的消费者
    }

    bool is_stopped() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return stopped_;
    }
};

// ============================================================
// 测试: 3 个生产者 + 3 个消费者，验证数据完整性
// ============================================================
int main()
{
    std::cout << "day04: multi-producer multi-consumer" << std::endl;

    using namespace std::chrono_literals;

    // 测试 1: 多生产者多消费者 — 验证数据完整性
    {
        MPMCQueue<int> que;

        constexpr int PRODUCERS          = 3;
        constexpr int CONSUMERS          = 3;
        constexpr int ITEMS_PER_PRODUCER = 10000;
        constexpr int TOTAL_ITEMS        = PRODUCERS * ITEMS_PER_PRODUCER;

        std::atomic<int> total_pushed{0};
        std::atomic<int> total_popped{0};

        std::vector<std::thread> producers;
        for (int i = 0; i < PRODUCERS; ++i) {
            producers.emplace_back([&que, &total_pushed, id = i, n = ITEMS_PER_PRODUCER]() {
                for (int j = 0; j < n; ++j) {
                    int val = id * n + j;
                    que.push(val);
                    total_pushed.fetch_add(1, std::memory_order_relaxed);
                }
                std::cout << "  producer[" << id << "] done, pushed " << n << " items\n";
            });
        }

        std::vector<std::thread> consumers;
        for (int i = 0; i < CONSUMERS; ++i) {
            consumers.emplace_back([&que, &total_popped, id = i]() {
                // 【BUG 复现】消费者退出条件必须与 que.pop() 内部唤醒条件一致
                // 原代码: while (!stop_test || !que.empty())
                //   stop_test 是外部原子标志，而 pop() 内部 cv_.wait() 检查的是 stopped_
                //   时序: 消费者被 !stop_test 放行 → 进入 pop() → cv_.wait() 等 stopped_
                //   → 主线程设 stop_test=true → 但 stopped_ 始终为 false → 消费者永久阻塞
                // 修复: 消费者通过 pop() 返回 nullopt 自然退出（que.stop() 统一设置 stopped_ + notify_all）
                while (true) {
                    auto val = que.pop();
                    if (!val.has_value()) // que.stop() 后 pop() 返回 nullopt
                        break;
                    total_popped.fetch_add(1, std::memory_order_relaxed);
                }
                std::cout << "  consumer[" << id << "] done\n";
            });
        }

        for (auto& t : producers)
            t.join();

        // 调用 stop() 而非设置外部 stop_test 标志:
        //   stop() 将 stopped_ = true + notify_all()，统一唤醒 cv_.wait() 中阻塞的消费者
        //   pop() 发现 stopped_ 且队列已空 → 返回 nullopt → 消费者退出循环
        que.stop();

        for (auto& t : consumers)
            t.join();

        std::cout << "  push总数: " << total_pushed.load() << ", pop总数: " << total_popped.load() << "\n";

        if (total_pushed.load() == total_popped.load()) {
            std::cout << "  [PASS] 数据完整性验证通过\n";
        } else {
            std::cout << "  [FAIL] 数据不完整! 丢失 " << total_pushed.load() - total_popped.load() << " 项\n";
        }
    }

    // 测试 2: 优雅退出 — 验证 stop() 后消费者能正常退出
    {
        MPMCQueue<int>   que;
        std::atomic<int> count{0};

        std::thread producer([&] {
            for (int i = 0; i < 100; ++i) {
                que.push(i);
                count.fetch_add(1, std::memory_order_relaxed);
            }
            std::cout << "  producer(graceful) done\n";
        });

        std::thread consumer([&] {
            int popped = 0;
            while (true) {
                auto val = que.pop();
                if (!val.has_value())
                    break;
                ++popped;
            }
            std::cout << "  consumer(graceful) popped " << popped << " items\n";
        });

        producer.join();
        que.stop();
        consumer.join();

        std::cout << "  [PASS] 优雅退出测试通过\n";
    }

    std::cout << "day04 all tests done.\n";
    return 0;
}

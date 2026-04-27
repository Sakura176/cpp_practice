#include <atomic>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

// ============================================================
// 【版本一】有问题的 ThreadSafeQueue —— 用于演示 TOCTOU 竞态
// ============================================================
// 问题:
//  1. front() 和 empty() 未加锁，形成数据竞争
//  2. 调用方必须 empty → front → pop 三步走，
//     但 empty() 返回 true 后到 front() 之间队列可能已被其他线程清空（TOCTOU）
//
// TOCTOU (Time Of Check to Time Of Use):
//  检查状态(empty) 和使用状态(front/pop) 不是原子的，中间窗⼝被破坏
// ============================================================

template<typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(std::initializer_list<T>&& que) : m_que(que) {}

    void push(T& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.push(val);
    }

    void pop()
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.pop();
    }

    // BUG: front() 未加锁，push/pop 可能同时修改队列 → data race
    T front() const
    {
        // std::lock_guard<std::mutex> l(mtx);
        return m_que.front();
    }

    // BUG: empty() 未加锁，同样形成 data race
    bool empty() const { return m_que.empty(); }

private:
    std::mutex    mtx;
    std::queue<T> m_que;
};

// ============================================================
// 【版本二】修正版 ThreadSafeQueueFix —— 核⼼思路:
//  1. "取值+删除"合并为一个原子操作 try_pop()
//  2. 调用方不再需要先检查 empty() 再操作
//  3. 使用 std::optional<T> 表达"可能为空"的语义
// ============================================================
// 知识点:
//  - std::optional<T>: C++17 引入，明确表示值可能存在也可能不存在
//    避免使用 bool + T 输出参数或抛异常的方式
//  - 最小化锁粒度: 只在真正访问共享数据的区间加锁
//  - 空初始化列表 -> 会自动推导为空的 initializer_list
//    默认构造被禁用，需要注意

template<typename T>
class ThreadSafeQueueFix
{
public:
    ThreadSafeQueueFix(std::initializer_list<T>&& il)
    {
        std::lock_guard<std::mutex> l(mtx);
        for (const auto& item : il) {
            que_.push(item);
        }
    }

    void push(const T& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        que_.push(val);
    }

    void push(T&& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        que_.push(std::move(val));
    }

    // try_pop: 将判空 + 取值 + 删除合并为原子操作，彻底消除 TOCTOU
    // 返回 std::nullopt 表示队列为空
    std::optional<T> try_pop()
    {
        std::lock_guard<std::mutex> l(mtx);
        if (que_.empty()) {
            return std::nullopt;
        }
        T val = std::move(que_.front());
        que_.pop();               // ← 必须 pop！之前的bug是漏掉了这行
        return val;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> l(mtx);
        return que_.empty();
    }

private:
    mutable std::mutex mtx;
    std::queue<T>      que_;
};

using namespace std::chrono_literals;

// ============================================================
// 测试1: 原始 ThreadSafeQueue 的 TOCTOU 竞态演示
// ============================================================

void test_queue_toctou_race()
{
    std::cout << "\n[测试1] ThreadSafeQueue TOCTOU 竞态测试\n";
    std::cout << "---------------------------------------------\n";

    ThreadSafeQueue<int> que({1, 2, 3, 4, 5});
    std::atomic<bool>    stop{false};
    std::atomic<int>     success_count{0};
    std::atomic<int>     crash_count{0};

    std::thread pusher([&que, &stop]() {
        int val = 100;
        while (!stop.load()) {
            int tmp = val++;
            que.push(tmp);
            std::this_thread::yield();
        }
    });

    std::thread popper([&que, &stop, &success_count, &crash_count]() {
        int loop = 0;
        while (!stop.load()) {
            if (!que.empty()) {
                // BUG: empty() 返回 true 到 front() 之间，另一线程可能 pop() 清空队列
                int val = que.front();  // 可能崩溃或读到脏数据
                que.pop();
                success_count++;
            }
            loop++;
            if (loop % 100 == 0)
                std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(500ms);
    stop.store(true);

    pusher.join();
    popper.join();

    std::cout << "  成功 pop 次数: " << success_count.load() << "\n";
    std::cout << "  检测到异常: " << crash_count.load() << "\n\n";

    std::cout << "  TOCTOU 问题:\n";
    std::cout << "  1. 线程 A 检查: if (!que.empty()) → true\n";
    std::cout << "  2. 线程 B 执行: que.pop() → 队列变空\n";
    std::cout << "  3. 线程 A 继续: que.front() → 未定义行为！\n\n";
}

// ============================================================
// 测试2: ThreadSafeQueueFix 修复后的表现
// ============================================================

void test_queue_fix_toctou_race()
{
    std::cout << "\n[测试2] ThreadSafeQueueFix TOCTOU 已修复\n";
    std::cout << "---------------------------------------------\n";

    ThreadSafeQueueFix<int> que({1, 2, 3, 4, 5});
    std::atomic<bool>       stop{false};
    std::atomic<int>        success_count{0};

    std::thread pusher([&que, &stop]() {
        int val = 100;
        while (!stop.load()) {
            int tmp = val++;
            que.push(tmp);
            std::this_thread::yield();
        }
    });

    std::thread popper([&que, &stop, &success_count]() {
        int loop = 0;
        while (!stop.load()) {
            // 直接调用 try_pop()，不需要先调 empty()
            // try_pop 内部原子地完成"判空+取值+删除"
            auto result = que.try_pop();
            if (result.has_value()) {
                success_count++;
            }
            loop++;
            if (loop % 100 == 0)
                std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(500ms);
    stop.store(true);

    pusher.join();
    popper.join();

    std::cout << "  成功 pop 次数: " << success_count.load() << "\n\n";
    std::cout << "  修复思路: try_pop 内部一次性加锁完成判断 + 取值 + 删除\n";
    std::cout << "  → TOCTOU 窗口被消除\n\n";
}

// ============================================================
// 测试3: 多消费者场景下的数据丢失
// ============================================================
// 即使队列本身线程安全，多消费者场景仍有问题:
//  两个消费者同时 try_pop 成功 → 各取一个，没问题
//  但如果一个消费者用 empty() 判断后再 pop，就回到 TOCTOU 问题了
//  正确做法: 直接用 try_pop()，无需先检查 empty()

void test_multi_consumer_data_loss()
{
    std::cout << "\n[测试3] 多消费者数据丢失测试\n";
    std::cout << "----------------------------------------\n";

    // 使用有问题的队列，演示数据丢失
    ThreadSafeQueue<int> que({1, 2, 3, 4, 5});
    std::atomic<int>     pushed_count{0};
    std::atomic<int>     popped_count{0};
    std::atomic<bool>    done{false};

    std::thread producer([&que, &pushed_count, &done]() {
        for (int i = 0; i < 100; ++i) {
            int val = i + 10000;
            que.push(val);
            pushed_count++;
        }
        done.store(true);
    });

    std::thread consumer([&que, &popped_count, &done]() {
        int local_popped = 0;
        while (!done.load() || !que.empty()) {
            if (!que.empty()) {
                // BUG: empty()+front() 之间其他线程可能已 pop
                try {
                    [[maybe_unused]] int val = que.front();
                    que.pop();
                    local_popped++;
                } catch (...) {
                }
            }
            std::this_thread::yield();
        }
        popped_count.store(local_popped);
    });

    producer.join();
    consumer.join();

    std::cout << "  生产者放入: " << pushed_count.load() << " 个\n";
    std::cout << "  消费者取出: " << popped_count.load() << " 个\n";
    std::cout << "  初始队列: 5 个元素\n";
    std::cout << "  预期总数: " << (pushed_count.load() + 5) << "\n";
    std::cout << "  实际总数: " << popped_count.load() << "\n\n";

    if (popped_count.load() < pushed_count.load() + 5) {
        std::cout << "  ❌ 数据丢失: "
                  << (pushed_count.load() + 5 - popped_count.load())
                  << " 个元素丢失! (TOCTOU 导致)\n";
    } else {
        std::cout << "  ✓ 未检测到数据丢失\n";
    }
}

// ============================================================
// 测试4: front() 未加锁导致的不一致
// ============================================================

void test_front_unprotected_access()
{
    std::cout << "\n[测试4] front() 未加锁导致的不一致\n";
    std::cout << "----------------------------------------\n";

    ThreadSafeQueue<int> que({1});
    std::atomic<bool>    stop{false};
    std::atomic<int>     read_count{0};

    std::thread pusher([&que, &stop]() {
        int val = 100;
        while (!stop.load()) {
            int tmp = val++;
            que.push(tmp);
            std::this_thread::yield();
        }
    });

    std::thread reader([&que, &stop, &read_count]() {
        while (!stop.load()) {
            if (!que.empty()) {
                // 即使 empty() 返回 true，front() 在其他线程 pop 后可能非法
                [[maybe_unused]] int current = que.front();
                read_count++;
            }
            std::this_thread::yield();
        }
    });

    std::thread popper([&que, &stop]() {
        while (!stop.load()) {
            if (!que.empty()) {
                que.pop();
            }
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(500ms);
    stop.store(true);

    pusher.join();
    reader.join();
    popper.join();

    std::cout << "  front() 读取次数: " << read_count.load() << "\n\n";
    std::cout << "  问题: front() 未加锁，与 push/pop 形成 data race\n";
    std::cout << "  用 TSan (-fsanitize=thread) 运行会报告 data race\n";
}

// ============================================================
// 核心总结:
//  线程安全队列的设计关键:
//   1. 所有公有接口内部统一加锁，调用方无需自己管理锁
//   2. 将"检查+操作"合并为原子的一个步骤，消除 TOCTOU 窗口
//   3. 优先使用 std::optional<T> 或 std::pair<bool, T> 返回状态，
//      避免抛出异常或使用输出参数
//   4. empty() 仅在加锁下有意义，脱离锁的 empty() 查询无实际价值
// ============================================================

int main(int argc, char* argv[])
{
    test_queue_toctou_race();
    test_queue_fix_toctou_race();
    test_multi_consumer_data_loss();
    test_front_unprotected_access();
    return 0;
}

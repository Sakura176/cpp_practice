#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

/**
 * @brief 线程安全队列（条件变量版）
 *
 * 与 Day03 相比新增:
 *  1. std::condition_variable —— 消费者可以阻塞等待，避免 busy-wait
 *  2. wait_and_pop() —— 阻塞直到队列非空
 *
 * ---- 知识点: std::condition_variable ----
 *  - wait(lock, predicate) 等价于 while (!predicate()) wait(lock);
 *    这称为"虚唤醒(spurious wakeup)"防护
 *  - 虚唤醒: 即使没有 notify，wait 也可能返回。所以必须用 predicate 重检查条件
 *  - notify_one(): 唤醒一个等待线程（信号量效应）
 *  - notify_all(): 唤醒所有等待线程，用于"停止"等广播场景
 *
 * ---- 知识点: unique_lock vs lock_guard ----
 *  - condition_variable::wait 需要 unique_lock，因为等待期间必须解锁
 *    让其他线程可以获取锁并修改条件
 *  - lock_guard 不提供 unlock() 接口，无法用于 CV
 *
 * ---- 知识点: 模板的惰性实例化 (Lazy Instantiation) ----
 *  类模板的成员函数只有在被 ODR-used 时才会实例化。
 *  这意味着即使某个成员函数有编译错误，只要不调用它就能通过编译。
 *  这是 Day04 中 wait_and_pop 的 front() 缺少括号但能编译的原因!
 *  强制实例化: 显式实例化模板或调用该函数就会暴露错误。
 */
template<typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() = default;

    ThreadSafeQueue(std::initializer_list<T> il)
    {
        std::lock_guard<std::mutex> l(mtx);
        for (const auto& item : il) {
            m_que.push(item);
        }
    }

    void push(const T& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.push(val);
        cv_.notify_one();  // 唤醒一个等待的消费者
    }

    void push(T&& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.push(std::move(val));
        cv_.notify_one();
    }

    /**
     * try_pop: 非阻塞尝试弹出, 返回 {是否成功, 值}
     *
     * 性能注意: 队列为空时返回 {false, T{}}，会默认构造一个 T。
     * 对于重型类型有开销，可用 std::optional<T> 优化（见 Day03）
     */
    std::pair<bool, T> try_pop()
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (m_que.empty()) {
            return {false, T{}};
        }
        T val = std::move(m_que.front());
        m_que.pop();
        return {true, std::move(val)};
    }

    /**
     * wait_and_pop: 阻塞直到队列非空，然后弹出一个元素
     *
     * 参数说明:
     *  - T& val: 输出参数，通过引用返回被弹出的值
     *  - 也可设计为直接返回值，但需要处理"队列被永久关闭"的情况
     *
     * 使用 unique_lock 的原因:
     *  - wait() 内部需要 unlock(&mtx) 让其他线程可以继续 push
     *  - 被唤醒后需要 re-lock 再检查条件
     */
    void wait_and_pop(T& val)
    {
        std::unique_lock<std::mutex> lock(mtx);
        // wait 接受 predicate 自动处理虚唤醒: while(!pred()) wait(lock);
        cv_.wait(lock, [this] { return !m_que.empty(); });
        val = std::move(m_que.front());  // 注意: front() 是函数调用，括号不能省略!
        m_que.pop();
    }

    // front() 仅供单线程调试用，多线程下即使加锁也有 TOCTOU 问题
    T front() const
    {
        std::lock_guard<std::mutex> l(mtx);
        return m_que.front();
    }

    // empty() 仅加锁时有意乪，脱离锁的 empty 查询无实际意义（TOCTOU）
    bool empty() const
    {
        std::lock_guard<std::mutex> l(mtx);
        return m_que.empty();
    }

private:
    mutable std::mutex      mtx;
    std::condition_variable cv_;
    std::queue<T>           m_que;
};

// ============================================================
// 生产者-消费者 (对应周五要求)
//
// 设计要点:
//  1. 使用 sentinel value（哨兵值，如 -1）通知消费者退出
//  2. wait_and_pop 阻塞等待，无 busy-wait
//  3. 无裸 new/delete
//  4. main 控制在 30 行左右
// ============================================================

int main()
{
    ThreadSafeQueue<int> que;

    // 生产者线程
    std::thread producer([&que] {
        for (int i = 1; i <= 10; ++i) {
            que.push(i);
            std::cout << "push: " << i << '\n';
        }
        que.push(-1);  // 哨兵值: 通知消费者退出
    });

    // 消费者线程: 使用 wait_and_pop，无 busy-wait
    std::thread consumer([&que] {
        while (true) {
            int val;
            que.wait_and_pop(val);
            if (val == -1) {  // 检测哨兵，退出
                std::cout << "pop: -1 (sentinel), exit.\n";
                break;
            }
            std::cout << "pop: " << val << '\n';
        }
    });

    producer.join();
    consumer.join();

    // 关键设计分析:
    //  1. 为什么用 sentinel 而不是 stop_flag + try_pop?
    //     - sentinel 保证消费者一定看到"任务已全部结束"的信号
    //     - stop_flag + try_pop 需要 busy-wait，或者 wait_for + 超时
    //  2. sentinel 的缺点: 需要选择一个不会出现在正常数据中的值
    //     - 替代方案: std::optional<T> + stop() + notify_all()
    //  3. 多个消费者需要多个 sentinel 或 stop() 广播
    //     - notify_all() + 停止标志

    return 0;
}

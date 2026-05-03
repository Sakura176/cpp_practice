/**
 * @brief 有界队列（满时生产者阻塞）
 *
 * ============================================================
 * 第 3 周 · 周五
 * 知识点: 有界缓冲区, 背压 (backpressure), 两个条件变量
 * ============================================================
 *
 * ---- 要求 ----
 * 实现一个有界阻塞队列:
 *  1. 固定容量（构造时指定）
 *  2. 队列满时生产者阻塞，直到消费者取走数据
 *  3. 队列空时消费者阻塞，直到生产者放入数据
 *  4. 支持优雅退出 stop()
 *
 * ---- 知识点: 为什么需要两个条件变量 ----
 *
 *  有界队列有两个阻塞条件:
 *    消费者等待: 队列非空（not_empty_）
 *    生产者等待: 队列未满（not_full_）
 *
 *  如果用同一个条件变量:
 *    push 后 notify_one — 可能唤醒另一个生产者（而不是消费者）！
 *    生产者检查 while (full()) → 继续等待 → 死锁风险。
 *
 *  因此需要两个条件变量分别通知:
 *    push 后 → not_empty_.notify_one() （唤醒一个消费者）
 *    pop 后 → not_full_.notify_one()  （唤醒一个生产者）
 *
 *  这个设计就是"条件变量的正确用法"——不同的条件用不同的 CV。
 *
 * ---- 背压 (Backpressure) ----
 *
 *  有界队列的核心价值:
 *    生产者速度 > 消费者速度 → 队列膨胀 → 内存 OOM
 *    有界队列让生产者阻塞 → 自然限流 → 系统稳定
 *
 *  这就是背压 —— 消费者处理能力不足时反向压制生产者。
 *  类似 TCP 的流量控制窗口。
 *
 * ---- 面试追问 ----
 *  - Q: 有界队列和无界队列怎么选？
 *    A: 任何生产环境都应使用有界队列。无界队列在流量尖峰时
 *       会导致 OOM，生产事故的常见原因。消息中间件（Kafka/RabbitMQ）
 *       的消费者处理不过来时，最终会触发"重平衡"或"踢出消费组"，
 *       本质上也是一种背压。
 *
 *  - Q: 两个条件变量会不会死锁？
 *    A: 如果 push 和 pop 的内部加锁顺序一致，不会死锁。
 *       加锁顺序必须保证一致（比如都先锁 mtx_）。
 *       条件变量本身不持有锁——wait 会释放锁，notify 不需要锁。
 *
 *  - Q: 如果所有生产者都阻塞了，消费者线程崩溃了怎么办？
 *    A: 系统死锁——生产者永远等 not_full，但不再有消费者 pop。
 *       解决方案: 用 stop() 广播 notify_all，生产者在 stopped 时
 *       抛异常或返回 false，不再阻塞。
 *
 *  - Q: 这个队列的容量应该设多大？
 *    A: 看业务: (1) 生产速率 × 消费者最大延迟 = 最小缓冲区大小。
 *       可参考 Little's Law 计算。经验值: CPU 密集 1~2 倍线程数，
 *       IO 密集 10~100 倍。通常通过压测调整。
 */

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

// ============================================================
// TODO: 实现 BoundedQueue
// ============================================================
// 模板参数: T — 元素类型
// 成员:
//   std::queue<T>        queue_;
//   std::mutex           mtx_;
//   std::condition_variable not_empty_;  // 消费者等待: 队列非空
//   std::condition_variable not_full_;   // 生产者等待: 队列未满
//   size_t               capacity_;      // 容量上限
//   bool                 stopped_{false};
//
// 接口:
//   void push(T value)      — while (full() && !stopped) not_full_.wait(lock);
//                             queue_.push(value); not_empty_.notify_one();
//   T pop()                 — while (empty() && !stopped) not_empty_.wait(lock);
//                             pop; not_full_.notify_one(); return;
//   void stop()             — stopped_ = true; not_empty_.notify_all(); not_full_.notify_all();
//
// 关键: push 和 pop 末尾必须 notify 另一个条件变量！
// ============================================================

// ============================================================
// TODO: main() — 演示背压效果
// ============================================================
int main()
{
    std::cout << "day05: bounded queue (backpressure)" << std::endl;

    // TODO: 创建 BoundedQueue<int>，容量设为 4
    // TODO: 慢消费者: pop 后 sleep 200ms
    // TODO: 快生产者: push 不 sleep
    // TODO: 观察队列满时生产者阻塞（背压效果）
    // TODO: 1 秒后 stop() 退出

    return 0;
}

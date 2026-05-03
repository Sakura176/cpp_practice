/**
 * @brief 生产者-消费者队列（信号量版）
 *
 * ============================================================
 * 第 3 周 · 周二
 * 知识点: C++20 std::counting_semaphore, 信号量与条件变量的对比
 * ============================================================
 *
 * ---- 要求 ----
 * 使用 C++20 std::counting_semaphore 实现线程安全的阻塞队列:
 *  1. 用信号量计数表示"队列中的元素个数"
 *  2. 生产者 push → sem_.release()（计数 +1）
 *  3. 消费者 pop → sem_.acquire()（计数 -1，阻塞直到 > 0）
 *  4. 结合 std::mutex 保护队列本身的并发访问
 *
 * ---- 知识点: std::counting_semaphore ----
 *
 *  信号量 vs 条件变量:
 *    条件变量: 需要 mutex + predicate + while 循环防虚唤醒
 *    信号量:   acquire/release 是原子操作，无虚唤醒问题
 *    结论:     信号量代码更简洁（条件变量需要 predicate 管理状态）
 *
 *  C++20 信号量接口:
 *    std::counting_semaphore<max> sem(N);  // N 为初始计数
 *    sem.acquire();   // count--，若 count == 0 则阻塞
 *    sem.release();   // count++，唤醒一个等待者
 *    sem.try_acquire(); // 非阻塞版
 *
 *  二值信号量 vs 计数信号量:
 *    std::binary_semaphore 是计数为 1 的特化: std::counting_semaphore<1>
 *    相当于轻量级的 mutex（但无所有权概念，可在不同线程 release）
 *
 * ---- 设计要点 ----
 *  信号量只解决"计数等待"问题，不保护共享数据。
 *  队列本身的 push/pop 仍需要 mutex 保护。
 *  信号量替代的是 condition_variable，不是 mutex。
 *
 * ---- 面试追问 ----
 *  - Q: 信号量相比条件变量的最大优势是什么？
 *    A: 不需要 predicate 和 while 循环防虚唤醒。信号量的 acquire/release
 *       是原子的，操作系统保证不会虚唤醒。代码更短，心智负担更小。
 *
 *  - Q: 为什么 C++20 之前没有信号量？之前怎么办？
 *    A: 之前可以用 POSIX sem_t（sem_wait/sem_post），或自己用
 *       mutex + condvar + counter 模拟信号量。C++20 将其标准化。
 *
 *  - Q: 用信号量实现有界队列（阻塞生产者）怎么做？
 *    A: 需要两个信号量: empty_sem（空位计数）+ full_sem（数据计数）。
 *       生产者 acquire(empty_sem), release(full_sem);
 *       消费者 acquire(full_sem), release(empty_sem)。
 *       这正是 day05 的内容。
 *
 *  - Q: semaphore 的 max 参数什么意思？
 *    A: 计数信号量的最大计数值。对于有界队列就是队列容量。
 *       acquire 永远不会让计数低于 0，release 永远不会超过 max。
 */

#include <iostream>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>

// ============================================================
// TODO: 实现 SemaphoreQueue
// ============================================================
// 模板参数: T — 队列元素类型
// 成员:
//   std::queue<T>                queue_;
//   std::mutex                   mtx_;
//   std::counting_semaphore<>    sem_;  // C++20, 需要 #include <semaphore>
//
// 接口:
//   void push(T value)  — mtx_.lock(); queue_.push(value); mtx_.unlock(); sem_.release();
//   T pop()             — sem_.acquire(); mtx_.lock(); pop; mtx_.unlock(); return value;
//                      注意顺序: 先等信号量再拿锁（避免持锁阻塞）
// ============================================================

// ============================================================
// TODO: main()
// ============================================================
int main()
{
    std::cout << "day02: counting_semaphore producer-consumer" << std::endl;

    // TODO: 创建 SemaphoreQueue<int>
    // TODO: 生产者消费者线程
    // TODO: 演示信号量版本比 condvar 版本更简洁

    return 0;
}

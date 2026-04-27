#ifndef SCOPED_MUTEX_H
#define SCOPED_MUTEX_H

#include <mutex>

/**
 * @brief 极简 ScopedMutex（类似 std::lock_guard）
 *
 * 要求:
 *  1. 模板类，可包装任意符合 BasicLockable 要求的类型
 *  2. 构造时调用 mtx.lock()
 *  3. 析构时调用 mtx_.unlock()
 *  4. 禁止拷贝/移动
 *
 * ---- 知识点 ----
 *
 *  RAII 加锁:
 *    lock_guard / unique_lock / scoped_lock (C++17) 都是 RAII 加锁的体现。
 *    核心思想: 构造 = 获取资源(加锁)，析构 = 释放资源(解锁)。
 *    异常安全——无论函数体是否抛出异常，析构都会执行。
 *
 *  BasicLockable 概念:
 *    一个类型满足 BasicLockable 当且仅当它有 lock() 和 unlock() 成员。
 *    std::mutex, std::timed_mutex, std::shared_mutex 等都满足。
 *
 *  为什么禁止移动?
 *    锁的语义是"谁加锁谁解锁"。移动意味着锁的所有权转移，
 *    虽然技术上可以实现（如 std::unique_lock），但 ScopedMutex 定位为
 *    最轻量的 RAII 包装，不支持移动。
 *
 *  对比 std::lock_guard:
 *    本质完全一样。lock_guard 还有构造函数可选 std::adopt_lock_t
 *    标签以接管已锁住的 mutex。这个练习不要求实现那个特性。
 */

template<typename Mutex>
class ScopedMutex
{
public:
    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    /**
     * 构造函数: 加锁
     * 要求: 调用 mtx.lock()
     */
    explicit ScopedMutex(Mutex& mtx);

    /** 析构函数: 解锁 */
    ~ScopedMutex();

    ScopedMutex(const ScopedMutex&)            = delete;
    ScopedMutex& operator=(const ScopedMutex&) = delete;

private:
    Mutex& mtx_; // 注意: 存储的是引用，不是指针
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================

template<typename Mutex>
ScopedMutex<Mutex>::ScopedMutex(Mutex& mtx) : mtx_(mtx)
{ mtx_.lock(); }

template<typename Mutex>
ScopedMutex<Mutex>::~ScopedMutex()
{ mtx_.unlock(); }
#endif // SCOPED_MUTEX_H

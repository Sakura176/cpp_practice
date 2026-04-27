/**
 * @brief ScopedMutex 测试套件（GTest）
 *
 * 测试覆盖:
 *  1. 构造加锁、析构解锁（通过 flag 验证）
 *  2. 与 std::mutex 配合使用
 *  3. 与 std::timed_mutex 配合使用（验证模板泛型）
 *  4. 异常安全：抛出异常时锁正确释放
 *  5. 禁止拷贝（编译期检查）
 */

#include <gtest/gtest.h>
#include "day02_scoped_mutex.h"

#include <mutex>
#include <thread>

// -----------------------------------------------------------
// 辅助: 跟踪 lock/unlock 调用的 BasicLockable
// -----------------------------------------------------------

struct TrackMutex
{
    int lock_count = 0;
    int unlock_count = 0;

    void lock() { lock_count++; }
    void unlock() { unlock_count++; }
};

// -----------------------------------------------------------
// 测试 1: 构造加锁、析构解锁
// -----------------------------------------------------------

TEST(Day02, lock_unlock_raii)
{
    TrackMutex mtx;
    EXPECT_EQ(mtx.lock_count, 0) << "not locked yet";
    EXPECT_EQ(mtx.unlock_count, 0) << "not unlocked yet";

    {
        ScopedMutex<TrackMutex> guard(mtx);
        EXPECT_EQ(mtx.lock_count, 1) << "locked in constructor";
        EXPECT_EQ(mtx.unlock_count, 0) << "not unlocked yet";
    }

    EXPECT_EQ(mtx.lock_count, 1) << "still 1 lock total";
    EXPECT_EQ(mtx.unlock_count, 1) << "unlocked in destructor";
}

// -----------------------------------------------------------
// 测试 2: 与 std::mutex 配合使用
// -----------------------------------------------------------

TEST(Day02, with_std_mutex)
{
    std::mutex mtx;
    int counter = 0;

    {
        ScopedMutex<std::mutex> guard(mtx);
        counter = 42;
    }

    mtx.lock();
    EXPECT_EQ(counter, 42) << "value set under lock";
    mtx.unlock();
}

// -----------------------------------------------------------
// 测试 3: 与 std::timed_mutex 配合（验证模板泛型）
// -----------------------------------------------------------

TEST(Day02, with_timed_mutex)
{
    std::timed_mutex tmtx;

    {
        ScopedMutex<std::timed_mutex> guard(tmtx);
    }
    EXPECT_TRUE(tmtx.try_lock()) << "timed_mutex unlocked by destructor";
    tmtx.unlock();
}

// -----------------------------------------------------------
// 测试 4: 异常安全 — 栈展开时析构仍会执行
// -----------------------------------------------------------

TEST(Day02, exception_safety)
{
    TrackMutex mtx;

    try {
        ScopedMutex<TrackMutex> guard(mtx);
        throw std::runtime_error("test error");
    } catch (...) {
    }

    EXPECT_EQ(mtx.lock_count, 1) << "locked before throw";
    EXPECT_EQ(mtx.unlock_count, 1) << "unlocked by destructor during stack unwinding";
}

// -----------------------------------------------------------
// 测试 5: 多线程验证互斥效果
// -----------------------------------------------------------

TEST(Day02, mutual_exclusion)
{
    std::mutex mtx;
    int shared_data = 0;
    constexpr int ITERS = 10000;

    std::thread t1([&]() {
        for (int i = 0; i < ITERS; ++i) {
            ScopedMutex<std::mutex> guard(mtx);
            shared_data++;
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < ITERS; ++i) {
            ScopedMutex<std::mutex> guard(mtx);
            shared_data++;
        }
    });

    t1.join();
    t2.join();

    EXPECT_EQ(shared_data, 2 * ITERS) << "no data race with ScopedMutex";
}

// -----------------------------------------------------------
// 测试 6: 禁止拷贝（编译期验证）
// -----------------------------------------------------------

TEST(Day02, no_copy)
{
    std::mutex mtx;
    ScopedMutex<std::mutex> guard(mtx);
    // ScopedMutex<std::mutex> guard2 = guard;  // 拷贝构造 → delete
    (void)guard;
}

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

/**
 * @brief 线程安全 Counter 类（互斥锁版本）
 *
 * 知识点:
 *  1. mutable std::mutex: 锁成员声明为 mutable，允许在 const 方法（如 get()）中加锁
 *  2. lock_guard: RAII 封装，构造加锁、析构解锁，异常安全
 *  3. get() 必须加锁: 不加锁时 reader 与 writer 形成数据竞争（即使只是读一个 long）
 *    - TSan 可以检测到这种 data race
 *
 * 注意:
 *  - 含 std::mutex 的类不可拷贝也不可移动（mutex 本身不可拷贝），
 *    建议显式声明 = delete 以避免隐式生成导致意外行为
 *  - increment()/decrement() 各自独立加锁，连续两次 increment()
 *    中间可能被其他线程插入，不保证结果为 +2
 *    → 如果需要原子化的复合操作，应在上层加锁
 */
class Counter
{
public:
    Counter() = default;

    // mutex 不可拷贝，显式删除拷贝/移动操作，意图更清晰
    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;
    Counter(Counter&&) = delete;
    Counter& operator=(Counter&&) = delete;

    void increment()
    {
        std::lock_guard<std::mutex> lock(mtx);
        count++;
    }

    void decrement()
    {
        std::lock_guard<std::mutex> lock(mtx);
        count--;
    }

    long get() const
    {
        // 读操作也需要加锁，否则与 increment/decrement 中的写形成数据竞争
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

private:
    long               count{0};
    mutable std::mutex mtx;
};

// ===================================================
// 测试代码
// ===================================================

void test_counter_get_race()
{
    std::cout << "\n[测试1] 数据竞争测试\n";
    std::cout << "-----------------------\n";

    Counter           counter;
    std::atomic<bool> stop{false};
    std::atomic<bool> read_count{0};

    // 写入线程
    std::thread writer([&counter, &stop]() {
        long c = 0;
        while (!stop.load()) {
            counter.increment();
            c++;
            if (c % 10000 == 0)
                stop.store(true);
        }
    });

    std::thread reader([&counter, &stop, &read_count]() {
        while (!stop.load()) {
            [[maybe_unused]] long val = counter.get();
            read_count.exchange(read_count.load() + 1);
        }
    });
    writer.join();
    stop.store(true);
    reader.join();

    std::cout << "  读取次数: " << read_count.load() << "\n";
    std::cout << "  注意: 使用 TSan 运行会报告 data race\n\n";
    std::cout << "  问题分析:\n";
    std::cout << "  1. 如果一个版本中 get() 没有加锁 → reader 与 writer 形成 data race\n";
    std::cout << "  2. 当前已修正: get() 也加锁保护\n";
    std::cout << "  3. 替代方案: 直接将 count 声明为 std::atomic<long>，无需 mutex\n";
}

int main(int argc, char* argv[])
{
    test_counter_get_race();
    return 0;
}

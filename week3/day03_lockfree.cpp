/**
 * @brief 无锁队列（SPSC: Single Producer Single Consumer）
 *
 * ============================================================
 * 第 3 周 · 周三
 * 知识点: std::atomic, 内存顺序, 环形缓冲区, ABA 问题
 * ============================================================
 *
 * ---- 要求 ----
 * 实现一个单生产者-单消费者的无锁环形缓冲区队列:
 *  1. 使用 std::atomic 管理读写指针
 *  2. 内部使用固定大小数组（环形缓冲区），避免动态内存分配
 *  3. 生产者写入 → 移动写指针；消费者读取 → 移动读指针
 *  4. 支持 try_push / try_pop（非阻塞）
 *
 * ---- 知识点: 无锁编程的内存顺序 ----
 *
 *  memory_order 的核心区别:
 *    relaxed     — 只保证原子性，不保证顺序（最轻量）
 *    acquire     — 本线程后续的读不能重排到此操作之前
 *    release     — 本线程之前的写不能重排到此操作之后
 *    acq_rel     — acquire + release（读-改-写操作）
 *    seq_cst     — 全局顺序一致（最重量，也是默认值）
 *
 *  SPSC 的生产者-消费者模型:
 *    生产者: write_index_.store(new_val, release)  — 确保数据写入先于指针发布
 *    消费者: read_index_.load(acquire)              — 确保指针获取后于数据读取
 *    这就是 "release-acquire 屏障": 生产者 release 之前的写入，
 *    对消费者 acquire 之后的读取可见。
 *
 *  为什么 SPSC 不需要 CAS?
 *    CAS (compare-and-swap) 用于多生产者/多消费者竞争同一指针。
 *    单生产者时，写指针只有一个人改；单消费者时，读指针只有一个人改。
 *    因此只需要 atomic load/store，不需要 read-modify-write。
 *
 * ---- 环形缓冲区要点 ----
 *
 *  缓冲区大小必须是 2 的幂（便于取模: index & (size-1) 代替 index % size）
 *  判满: (write_index - read_index) == size
 *  判空: write_index == read_index
 *
 * ---- 面试追问 ----
 *  - Q: 为什么内存顺序用 release/acquire 而不是 seq_cst？
 *    A: seq_cst 在所有架构上生成最重的内存屏障指令（通常是 full barrier）。
 *       x86 上 release/acquire 几乎零开销（x86 的 store 自带 release 语义），
 *       ARM/PowerPC 上需要显式屏障指令。release/acquire 足够保证 SPSC 的正确性。
 *
 *  - Q: 什么是 ABA 问题？在 SPSC 中存在吗？
 *    A: ABA 问题发生在 CAS 操作中: 值从 A→B→A，CAS 认为没变过。
 *       SPSC 的读写指针没有被多个线程竞争修改，不存在 ABA 问题。
 *       但 MPSC/MSPMC 的无锁队列中 ABA 是一个核心难题。
 *
 *  - Q: 这个队列是"无锁"的吗？是"无等待"的吗？
 *    A: 是"无锁"的（lock-free）—— 至少有一个线程能推进。
 *       但不是"无等待"的（wait-free）—— 线程可能因为缓冲区满/空而忙等待。
 *       在 SPSC 中，生产者和消费者相互依赖对方的进度。
 *
 *  - Q: 如何扩容？（这个版本是固定大小）
 *    A: 固定大小是 SPSC 的常见选择（高频场景通常可预测最大负载）。
 *       如需动态扩容，可以使用读时拷贝（RCU）或二级链表结构。
 *       但动态分配和无锁是一对矛盾——需要内存回收机制（Hazard Pointer / EBR）。
 */

#include <atomic>
#include <iostream>
#include <thread>

// ============================================================
// TODO: 实现 SPSCQueue (Ring Buffer)
// ============================================================
// 模板参数: T — 元素类型, Size — 缓冲区大小（应为 2 的幂）
// 成员:
//   T                buffer_[Size];   // 环形缓冲区
//   std::atomic<size_t> write_index_; // 生产者写指针
//   std::atomic<size_t> read_index_;  // 消费者读指针
//
// 接口:
//   bool try_push(const T& value) — 判满 → 写入 → write_index_++
//   bool try_pop(T& value)        — 判空 → 读取 → read_index_++
//   size_t size() const           — 当前元素个数
//   bool empty() const            — 判空
//   bool full() const             — 判满
//
// 注意:
//   标准环形缓冲区判满留一个空位: (write_index - read_index) == Size
//   或者维护 count_ 原子变量（略增加开销）
// ============================================================
template<typename T>
class SPSCQueue
{
    static const int MAX_BUF_SIZE = 1024;

private:
    T                   buffer_[MAX_BUF_SIZE];
    std::atomic<size_t> write_index_;
    std::atomic<size_t> read_index_;

public:
    bool try_push(const T& value)
    {
        int write_index = write_index_.load(std::memory_order_acquire);
        int read_index  = read_index_.load(std::memory_order_acquire);
        if (write_index >= MAX_BUF_SIZE) {
            write_index &= (MAX_BUF_SIZE - 1);
            write_index_.exchange(write_index, std::memory_order_acq_rel);
        }

        if (full())
            return false;

        buffer_[write_index] = value;
        write_index_.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }

    bool try_pop(T& value)
    {
        int write_index = write_index_.load(std::memory_order_acquire);
        int read_index  = read_index_.load(std::memory_order_acquire);
        if (read_index >= MAX_BUF_SIZE) {
            read_index &= (MAX_BUF_SIZE - 1);
            read_index_.exchange(read_index, std::memory_order_acq_rel);
        }

        if (empty()) {
            return false;
        }

        value = buffer_[read_index];
        read_index_.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }

    size_t size() const
    { return write_index_.load(std::memory_order_acquire) - read_index_.load(std::memory_order_acquire); }

    bool empty() const
    { return write_index_.load(std::memory_order_acquire) - read_index_.load(std::memory_order_acquire) == 0; }

    bool full() const
    {
        return write_index_.load(std::memory_order_acquire) - read_index_.load(std::memory_order_acquire) ==
               MAX_BUF_SIZE;
    }
};

// ============================================================
// TODO: main() — 1 个生产者线程 + 1 个消费者线程
// ============================================================
int main()
{
    std::cout << "day03: lock-free SPSC ring buffer" << std::endl;

    // TODO: 创建 SPSCQueue<int, 1024>
    // TODO: 生产者连续写入数据
    // TODO: 消费者连续读取数据
    // TODO: 验证所有数据被正确消费（没有丢失、没有错序）
    // TODO: 对比条件变量版本的性能

    SPSCQueue<int> que;

    std::thread p_thread([&] {
        int count = 0;
        while (true) {
            que.try_push(count);
            std::cout << "p_thread try push val: " << count++ << "\n";
        }
    });

    std::thread c_thread([&] {
        while (true) {
            int val = -1;
            que.try_pop(val);
            std::cout << "c_thread try pop val: " << val << "\n";
        }
    });

    p_thread.join();
    c_thread.join();

    return 0;
}

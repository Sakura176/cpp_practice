# Code Review: Week 3 Day 03 — 无锁队列（SPSC 环形缓冲区）

**评审日期**: 2026-04-28 → 2026-05-05
**评审人**: Claude (Senior C++ Engineer)

---

## 整体评价

代码实现了 SPSC 环形缓冲区的基本结构，但**索引管理方案存在根本性设计错误**——对单调递增索引进行就地 wrap 破坏了环形缓冲区的核心不变量。TSAN 确认了 data race，且数据读取已因索引错乱而读取到**过时约 1024 个元素**的脏数据。作为教学练习，这是一个极好的"看似正确实则全错"的无锁编程反面案例。

---

## 逐项评审

### 1. [严重] 索引就地 wrap 破坏了核心不变量

```cpp
bool try_push(const T& value) {
    int write_index = write_index_.load(memory_order_acquire);
    int read_index  = read_index_.load(memory_order_acquire);
    if (write_index >= MAX_BUF_SIZE) {                    // ← 错误!
        write_index &= (MAX_BUF_SIZE - 1);
        write_index_.exchange(write_index, acq_rel);       // ← 修改原子变量!
    }
    ...
}

bool full() const {
    return write_index_.load(acquire) - read_index_.load(acquire) == MAX_BUF_SIZE;
    //    ^^^^^^^^^ 被 exchange 重置为小值    ^^^^^^^^^ 仍然是原来的大值
    //    → 相减溢出（unsigned 下溢）→ 永远 != MAX_BUF_SIZE
}
```

**问题**: SPSC 环形缓冲区的标准设计是**索引单调递增，永不回绕**，通过 `index & (Size-1)` 计算缓冲区槽位。这样做减法判满/判空始终正确。

当前代码在索引达到 `MAX_BUF_SIZE` 时通过 `exchange()` 将其重置为 `index & (Size-1)`（小值），导致：
- `write_index_` 变成小值（如 519），而 `read_index_` 仍是 571600
- `write_index_ - read_index_` 发生**无符号下溢**，得到一个巨大值
- `full()` 总是返回 `false` → **缓冲区满时继续写入，覆盖未读数据**

**正确实现**:
```cpp
static constexpr size_t MASK = Size - 1;  // Size 必须是 2 的幂

bool try_push(const T& value) {
    size_t w = write_index_.load(std::memory_order_relaxed);
    size_t r = read_index_.load(std::memory_order_acquire);
    if (w - r >= Size) return false;       // 满
    buffer_[w & MASK] = value;
    write_index_.store(w + 1, std::memory_order_release);
    return true;
}
```

**永远不要修改单调计数器**。这是 SPSC 实现的第一原则。

---

### 2. [严重] `try_pop()` 在队列空时返回未初始化的值

```cpp
int val = -1;              // 调用方设置"默认值"
que.try_pop(val);          // 返回 false，val 未被修改
std::cout << val << "\n";  // 输出 -1（混淆数据）
```

以及实现中：
```cpp
bool try_pop(T& value) {
    ...
    if (empty()) return false;   // value 未被修改
    value = buffer_[read_index]; // 仅在成功时赋值
    ...
}
```

**问题**: `try_pop` 返回 `false` 时 `value` 引用不变。调用方直接将 `val` 值作为有效数据打印。正确的做法是仅在 `try_pop` 返回 `true` 时才使用结果。

更安全的 API 设计：`std::optional<T> try_pop()`，或检查返回值再使用。

---

### 3. [严重] 程序无法退出（无限循环）

```cpp
std::thread p_thread([&] {
    while (true) { que.try_push(count); ... }  // 永不停止
});
std::thread c_thread([&] {
    while (true) { que.try_pop(val); ... }    // 永不停止
});
p_thread.join();  // 永远阻塞
c_thread.join();  // 永远不会执行到这里
```

程序只能被 `timeout` 命令或 `SIGKILL` 终止。没有 `stop()` 机制，没有优雅退出。这与 day01/day02 的 "support stop" 要求相悖。

---

### 4. [严重] 100% CPU 忙等待

两个线程都在 `while (true)` 中无休眠调用 `try_push`/`try_pop`。即使队列满或空，函数立即返回并继续循环。这会导致：
- 一个 CPU 核心满载（生产者不断尝试 push）
- 另一个 CPU 核心满载（消费者不断尝试 pop）
- 完全没有退让（没有 `std::this_thread::yield()` 或睡眠）

--- 

### 5. [严重] TSAN 确认 Data Race

```
WARNING: ThreadSanitizer: data race
  Write of size 4 at buffer_[write_index] by producer thread
    #0 SPSCQueue<int>::try_push() day03_lockfree.cpp:111
```

由于索引管理方案错误，生产者和消费者操作相同的缓冲区槽位但未通过 release-acquire 正确同步。`exchange()` 在中间重置索引破坏了 happens-before 链。

--- 

### 6. [中等] 类型混用：`int` 承载 `size_t`

```cpp
int write_index = write_index_.load(std::memory_order_acquire);  // atomic<size_t> → int
int read_index  = read_index_.load(std::memory_order_acquire);
```

- `write_index_` 是 `std::atomic<size_t>`，但读取到 `int`（可能 32 位）
- `size_t` 在 64 位系统上是 64 位的，`int` 是 32 位的
- 当索引超过 2^31（约 21 亿）时，高 32 位被截断，值变负或错误

在当前测试中由于 1 秒内只跑 ~57 万次，未触发此问题。但如果长时间运行会被截断。

--- 

### 7. [中等] `exchange()` 和 `acq_rel` 使用过重

```cpp
write_index_.exchange(write_index, std::memory_order_acq_rel);
write_index_.fetch_add(1, std::memory_order_acq_rel);
```

SPSC 中生产者是唯一写入者，不需要 `exchange()`（`store()` 即可），也无需用 `acq_rel`（`release` 足以保证数据可见性）。更轻量的版本：

```cpp
write_index_.store(w + 1, std::memory_order_release);
```

--- 

### 8. [低] `const int MAX_BUF_SIZE` 不是 2 的幂检查

```cpp
static const int MAX_BUF_SIZE = 1024;
```

注释要求"缓冲区大小必须是 2 的幂"，但代码没有编译期检查。可以使用 `static_assert` 验证：

```cpp
static constexpr int MAX_BUF_SIZE = 1024;
static_assert((MAX_BUF_SIZE & (MAX_BUF_SIZE - 1)) == 0, "Size must be power of 2");
```

--- 

### 9. [观察] 正确顺序的 SPSC 参考

```cpp
template<typename T, size_t Size>
class SPSCQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    T buffer_[Size];
    std::atomic<size_t> write_{0};
    std::atomic<size_t> read_{0};
    static constexpr size_t MASK = Size - 1;

public:
    bool try_push(const T& val) {
        size_t w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= Size)
            return false;
        buffer_[w & MASK] = val;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& val) {
        size_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire))
            return false;
        val = buffer_[r & MASK];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }
};
```

关键区别：
- 索引**永不回绕**，选择 `Size` 的幂次确保 `& MASK` 正确
- 不使用 `exchange()`（SPSC 不需要 CAS）
- 生产者读 `read_` 用 `acquire`，写 `write_` 用 `release`
- 消费者读 `write_` 用 `acquire`，写 `read_` 用 `release`

--- 

## 测试结果

| 指标 | 值 |
|------|------|
| TSAN 数据竞争 | **2 次确认**（buffer 写 vs 读） |
| 生产者推送 | ~571k 项 |
| 消费者读取 | ~271k 项（含大量 -1） |
| 数据完整性 | **损坏**——消费者读到过时数据 |
| CPU 利用率 | 200%（两个核心满载） |
| 退出方式 | `timeout 2` 强制终止 |

--- 

## 面试扩展话题

### 1. "release-acquire 如何保证 SPSC 的正确性？"

生产者先写 `buffer_[i]`（普通写），再 `write_.store(i+1, release)`。release 保证所有之前的普通写在此操作之前完成。消费者 `write_.load(acquire)` 保证读到最新的索引，且看到该索引之前的所有内存写入。这就建立了 **happens-before** 关系。

### 2. "为什么 SPSC 不需要 CAS？"

CAS（compare-and-swap）解决的是多线程同时修改同一内存位置时的竞争问题。SPSC 中写索引只被生产者修改，读索引只被消费者修改，不存在竞争。因此普通的原子 load/store 就足够了。

### 3. "SPSC 的假共享（false sharing）问题？"

`write_` 和 `read_` 如果位于同一缓存行（通常 64 字节），写 `write_` 会使包含 `read_` 的缓存行失效，导致在两个核心之间频繁同步。应该用 padding 将它们分隔到不同缓存行：

```cpp
alignas(64) std::atomic<size_t> write_{0};
alignas(64) std::atomic<size_t> read_{0};
```

### 4. "如何给 SPSC 添加 backpressure？"

两个方向：
- **生产者阻塞**: 在 `try_push` 返回 false 时 `yield()` 或 `sleep()`
- **消费者阻塞**: 在 `try_pop` 返回 false 时 `yield()` 或 `sleep()`
- 或者混合使用信号量：仍用原子变量管理索引，用信号量计数元素个数（但这引入了锁）

纯无锁的 SPSC 通常没有阻塞语义——调用方通过循环重试来处理满/空。

--- 

## 总结

| 项目 | 评价 |
|------|------|
| 正确性 | **严重损坏**——索引 wrap 破坏了判满/判空逻辑，数据读到脏数据，TSAN 确认 data race |
| 无锁设计 | 对 SPSC 的理解有致命偏差——索引不应被 `exchange()` 重置 |
| 接口设计 | `try_pop` 返回 false 时不修改 value，调用方错误地使用未修改的值 |
| 优雅退出 | ❌ 无 stop 机制 |
| CPU 效率 | ❌ 100% 忙等待 |
| 内存序使用 | release/acquire 方向大部分正确，但被 wrap 逻辑破坏 |

**总体评价**: 这是 week3 中问题最多的练习。核心概念（环形缓冲区 + SPSC 无锁）是正确的，但**索引管理方案有根本性错误**：标准 SPSC 的中心思想是"索引永远向前，用位掩码算槽位"。当前代码试图回绕索引本身，导致 `full()`/`empty()` 检测完全失效——数据被覆盖、脏数据被读取。加上缺乏退出机制和 100% CPU 忙等待，这个队列在工程上是不可用的。好消息是，理解了这个 bug 之后，对 SPSC 的正确设计会有更深的认识（这正是"面试官追问"想要考察的）。

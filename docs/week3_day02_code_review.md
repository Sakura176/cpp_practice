# Code Review: Week 3 Day 02 — 生产者-消费者队列（信号量版）

**评审日期**: 2026-04-28
**评审人**: Claude (Senior C++ Engineer)

---

## 整体评价

相比 day01，本版有明显的工程改进：`stopped_` 改用了 `std::atomic<bool>` 消除了 data race，`empty()` 和 `size()` 加锁保护。但引入了信号量带来的新问题——**优雅退出无法实现**和**信号量最大值限制**是结构性缺陷。代码能"跑通"依赖运行时巧合，而非设计保证。

---

## 逐项评审

### 1. [严重] `std::counting_semaphore<100>` 最大值过小

```cpp
std::counting_semaphore<100> sem_;  // 最大计数值 = 100
```

**问题**: `std::counting_semaphore<100>` 的模板参数是最大计数值。当生产者推送速度超过消费者，队列长度超过 100 时，第 101 次 `sem_.release()` 会抛出 `std::system_error` → 程序崩溃。

在 1 秒的测试中，生产者推送了 ~20 万个元素。如果消费者被调度器短暂挂起（OS 上下文切换），队列可在数微秒内积压数百个元素，瞬间触发崩溃。

**修复**: 对于无界队列，应使用 `std::counting_semaphore<>`（默认模板参数由实现定义，通常为 `PTRDIFF_MAX`）：

```cpp
std::counting_semaphore<> sem_;
```

**面试追问**: *"countint_semaphore 模板参数的作用是什么？"*
- 模板参数 `MAX` 表示信号量的最大计数值
- `sem_.release(n)` 会将计数增加 n，但如果超过 MAX 则抛出异常
- 默认值由实现定义（≥ 32767），通常接近 `PTRDIFF_MAX`
- 在消息队列等有背压的场景中，这个参数可作为容量上限

---

### 2. [严重] `pop()` 无法响应停止信号 → 线程可能永远阻塞

```cpp
T pop()
{
    sem_.acquire();  // ← 如果信号量计数为 0，线程在此阻塞等待，无法被 stop() 唤醒
    mtx_.lock();
    auto val = queue_.front();
    queue_.pop();
    mtx_.unlock();
    return val;
}
```

**问题**: `sem_.acquire()` 是一个**不可中断的阻塞操作**。当 `stop()` 被调用后：
- 如果队列中仍有数据 → drain 工作（信号量计数 = 剩余元素数），正确
- 如果队列已空，但某个消费者已进入 `pop()` → `sem_.acquire()` 永远无法返回 → **线程挂死**

条件变量通过 `notify_all()` + predicate 可以优雅唤醒，但信号量的 `acquire()` 没有等效机制。

**场景重现**: 两个消费者，生产者停止后：
1. 消费者 A 获得最后一个元素（`sem_.acquire()` 成功）
2. 消费者 B 调用 `sem_.acquire()` → 计数为 0 → 阻塞
3. A 处理完毕，退出循环（`empty()` 为空）
4. B 永远阻塞，主线程 `join()` 永远等待 → **死锁**

**三种修复方案**:

| 方案 | 描述 | 优劣 |
|------|------|------|
| 方案 A: Poison Pill | 停止时 push 一个特殊值，消费者识别后退出 | 简单，但需要定义哨兵值 |
| 方案 B: `try_acquire` 轮询 | 循环 `try_acquire()` + `sleep`，检查 `stop_` | 简单但有忙等待 |
| 方案 C: 混合 CV | 放弃纯信号量，回退到 day01 的 condvar 方案 | 代码更复杂 |

---

### 3. [严重] `push()` 不是异常安全的

```cpp
void push(const T& value)
{
    mtx_.lock();
    queue_.push(value);   // ← 可能抛 std::bad_alloc
    mtx_.unlock();         // ← 如果上面抛异常，这行不会执行
    sem_.release();
}
```

**问题**: `std::queue::push()` 在内存不足时抛出 `std::bad_alloc`。此时 `mtx_` 未被 `unlock()` → mutex 永久锁定 → 任何其他线程尝试加锁时死锁。

**修复**: 使用 `std::lock_guard`（RAII 锁）确保异常安全：

```cpp
void push(const T& value)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(value);
    }
    sem_.release();
}
```

---

### 4. [中等] 多消费者 drain 阶段的竞争条件

```cpp
while (!que.is_stop() || !que.empty()) {  // 条件检查
    int val = que.pop();                   // 实际取数据
}
```

**问题**: `empty()` 的检查与 `pop()` 的执行不是原子的。在多消费者场景中：

1. Consumer A: `empty()` → false → 进入循环
2. Consumer B: `empty()` → false → 进入循环
3. A: `pop()` → `sem_.acquire()` → 获得最后一个元素
4. B: `pop()` → `sem_.acquire()` → 计数为 0 → ⚠️ **永远阻塞**

这是"TOCTOU"（Time-of-check Time-of-use）竞态条件——检查时条件成立，使用时已不成立。

**注意**: 单消费者场景下此问题不存在。但当前代码启动了 2 个消费者。

**修复**: 使用 `try_pop()` 替代 drain 中的阻塞 `pop()`，或使用 poison pill 模式。

---

### 5. [低] 手动 `lock()`/`unlock()` 代替 `lock_guard`

```cpp
mtx_.lock();
queue_.push(value);
mtx_.unlock();
```

RAII 是 C++ 资源管理的核心原则。`std::lock_guard` 不仅更简化代码，更重要的是提供了异常安全。虽然 `queue_.pop()` 和 `queue_.front()` 通常不抛异常，但 `queue_.push()` 可能抛异常（问题 3）。

---

### 6. [低] `#include <cstdio>` 未使用

第 59 行的 `#include <cstdio>` 没有被使用，应移除。

---

### 7. [样式] 信号量选择的合理性讨论

信号量相比条件变量的两大缺陷：
1. **不支持优雅退出** — acquire 不可中断
2. **最大值限制** — 无界队列无法使用固定最大值

信号量适用于**有界队列**（day05 的内容）、**资源池**（连接池、线程池）等场景。对于支持 graceful shutdown 的无界队列，条件变量是更好的选择。

---

## 相比 day01 的改进

| 问题 | day01 | day02 |
|------|-------|-------|
| `stopped_` 同步 | ❌ data race | ✅ `std::atomic<bool>` |
| `empty()` / `size()` 加锁 | ❌ data race | ✅ `lock_guard` |
| `mutable` 互斥量 | — | ✅ 允许 const 方法加锁 |
| 异常安全 | ✅ `lock_guard` | ❌ 手动 `lock()` |
| 优雅退出 | ✅ `notify_all` | ❌ sem_acquire 阻塞 |
| 代码简洁度 | 需 condvar predicate | ✅ 信号量语义直接 |

---

## 面试扩展话题

### 1. "信号量和条件变量的本质区别是什么？"

信号量是一个**计数器 + 等待队列**，condition_variable 是一个**等待队列 + 条件检查**。信号量的 release/acquire 自带状态（计数），条件变量需要外部状态（predicate）。

### 2. "C++20 信号量的性能如何？"

`std::counting_semaphore` 在 Linux 上基于 `futex` 实现，与 `std::mutex` 和 `condition_variable` 同级别。在无竞争时（计数 > 0），`acquire` 只是原子递减，不进入内核。与 condvar 性能相当。

### 3. "什么场景适合用信号量代替条件变量？"

- 资源池管理（固定数量资源的分配/回收）
- 有界队列（两个信号量实现背压）
- 不需要优雅退出的场景
- 需要"一次性唤醒"语义（信号量自带计数，不怕错过 notify）

### 4. "如何给信号量加超时？"

`sem.try_acquire_for(rel_time)` 或 `sem.try_acquire_until(abs_time)` 返回 `bool`，超时返回 `false`。可以用来实现有限的优雅退出（定期检查停止标志 + try_acquire）。

---

## 总结

| 项目 | 评价 |
|------|------|
| 正确性 | **3 个严重问题**：semaphore max 过小、无法优雅退出、push 异常不安全 |
| 同步设计 | `atomic<bool>` + mutex 使用正确，`mutable` 用法好 |
| 优雅退出 | **结构缺陷**：sem_acquire 不可中断，退出路径不完整 |
| 异常安全 | ❌ 手动 `lock()` 在 push 中可能导致死锁 |
| 代码风格 | 简洁，相比 condvar 版本代码量减少，但 `cstdio` 未移除 |
| 测试运行 | 未触发 TSAN（未达到触发条件），但设计层面的 bug 是确认的 |

**总体评价**: 相比 day01 在数据竞争防护上有明显进步（atomic flag、加锁查询），但信号量的核心缺陷限制了它的可用性：**最大值限制**把一个"无界队列"变成了"有界队列"，而 **acquire 不可中断**使得优雅退出不可靠。这个练习很好地展示了为什么生产环境中信号量不常用于生产者-消费者队列——虽然在某些简化场景下（固定大小的消息池）信号量很合适，但对于需要 graceful shutdown 的通用队列，条件变量是更安全的选择。

# 第一周代码审查报告

> **考察范围**: 基础封装、线程安全、线程安全队列、条件变量、生产者-消费者
> **检验标准**: 无全局变量、调用方无直接加锁/解锁
> **审查视角**: 资深 C++ 工程师 + 面试官

---

## 目录

1. [总体评价](#1-总体评价)
2. [Day01 — Counter 基础封装](#2-day01--counter-基础封装)
3. [Day02 — Counter + std::mutex](#3-day02--counter--stdmutex)
4. [Day03 — ThreadSafeQueue](#4-day03--threadsafequeue)
5. [Day04 — wait_and_pop + 生产者-消费者](#5-day04--wait_and-pop--生产者-消费者)
6. [共性问题总结](#6-共性问题总结)
7. [面试官追问题库](#7-面试官追问题库)
8. [修复记录](#8-修复记录)

---

## 1. 总体评价

### 优点

| 方面 | 评价 |
|------|------|
| 封装意识 | 所有数据成员均为 `private`，通过公有接口访问 ✅ |
| 安全性意识 | Day02 到 Day04 一直在关注线程安全问题 ✅ |
| 问题发现 | 自己写出了有 TOCTOU 问题的版本并尝试测试验证 ✅ |
| 工具链 | CMake 配置 TSan (`-fsanitize=thread`) 是好的实践 ✅ |
| 代码结构 | 每个文件独立可运行，单个 `.cpp` 自包含 ✅ |

### 不足之处

| 问题 | 严重程度 | 涉及文件 |
|------|---------|---------|
| `try_pop` 没有真正移除元素 | 🔴 P0 逻辑错误 | day03 |
| `wait_and_pop` 编译错误（`front` 缺括号） | 🔴 P0 编译错误 | day04 |
| `try_pop` 中 `int` 硬编码应为 `T` | 🔴 P0 模板错误 | day03 |
| `empty()` 未加锁导致 data race | 🟡 P1 | day03 (原始版), day04 |
| 未使用的 include/using | 🟢 P2 | day02 |
| 缺少拷贝/移动操作的显式声明 | 🟢 P2 | day02 |
| 消费者 busy-wait 而不是阻塞等待 | 🟢 P2 | day04 |
| `std::endl` 滥用 | 🟢 P2 代码风格 | day01 |

---

## 2. Day01 — Counter 基础封装

### 文件: `week1/day01_counter.cpp`

### 原始代码

```cpp
class Counter
{
public:
    void increment() { count++; }
    void decrement() { count--; }
    long get() const { return count; }

private:
    long count{0};
};
```

### 审查意见

**做得好的地方:**
- 使用列表初始化 `count{0}` ✅ — C++11 统一初始化语法，禁止窄化转换（如 `double`→`int` 会编译报错）
- 数据 `private`，通过公有接口访问 ✅ — 基础封装
- `get()` 标记为 `const` ✅ — const 正确性

**知识点分析:**

1. **`count++` 不是原子的**
   - 汇编层面是 `load → add → store` 三步
   - 单线程没问题，多线程必然数据竞争
   - 面试追问: "两个线程同时调用 `increment()` 100 次，结果一定是 200 吗？" → 不一定，可能介于 100 和 200 之间

2. **`std::endl` 与 `'\n'` 的区别**
   - `std::endl` = `'\n'` + `flush()` 刷新缓冲区
   - 非交互场景下（如写入文件、日志）频繁 flush 会大幅降低性能
   - 仅在需要强制输出（如日志崩溃前、交互式提示）时使用 `std::endl`
   - 这是很多 C++ 面试中的"代码异味"识别题

3. **`main` 的签名**
   - 不使用 `argc/argv` 时应写 `int main()` 而非 `int main(int argc, char* argv[])`
   - 未使用的参数编译器不会报错但会降低代码整洁度
   - 也可以 `(void)argc; (void)argv;` 显式标记未使用

### 改进后的代码

```cpp
class Counter
{
public:
    void increment() { count++; }
    void decrement() { count--; }
    long get() const { return count; }

private:
    long count{0};
};

int main()
{
    Counter count;
    count.increment();
    std::cout << "count: " << count.get() << '\n';  // endl → '\n'
    count.decrement();
    std::cout << "count: " << count.get() << '\n';
    return 0;
}
```

---

## 3. Day02 — Counter + std::mutex

### 文件: `week1/day02_counter_mutex.cpp`

### 原始代码 (修复前)

```cpp
#include <atomic>      // 未使用
#include <iostream>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;  // 未使用

class Counter
{
public:
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
        // 早期版本中 get() 未加锁 —— 这是 bug
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

private:
    long               count{0};
    mutable std::mutex mtx;
};
```

### 审查意见

**做得好的地方:**
- `mutable std::mutex mtx` ✅ — mutex 必须为 `mutable` 才能在 `const` 方法中加锁
- `get()` 已经加锁 ✅ — 避免 reader 与 writer 的数据竞争
- 使用 `std::lock_guard` ✅ — RAII 封装，异常安全

**知识点分析:**

1. **为什么 `mutex` 要声明为 `mutable`？**
   - `get() const` 承诺不修改对象状态（`this` 为 `const Counter*`）
   - 但加锁和解锁会修改 `mtx` 的内部状态
   - `mutable` 允许在 `const` 方法中修改特定成员
   - 面试追问: "`mutable` 还能用在哪些场景？" → 缓存、引用计数、调试计数器

2. **`std::lock_guard` vs `std::unique_lock`**
   - `lock_guard`: 构造加锁、析构解锁，没有额外接口 —— 轻量，首选
   - `unique_lock`: 可提前解锁、可转移所有权、可与条件变量配合 —— 重量级，只在需要时才用
   - 面试考点: 条件变量为什么必须用 `unique_lock`？ → `wait()` 内部需要 unlock

3. **含 `mutex` 的类不可拷贝**
   - `std::mutex` 不可拷贝、不可移动
   - 编译器会隐式删除 `Counter` 的拷贝构造/拷贝赋值
   - 但**隐式删除**可能导致意外：一个成员加了 mutex，整个类突然不能拷贝了
   - **最佳实践**: 显式 `= delete` 让意图清晰

4. **锁粒度问题**
   - `increment()` 和 `decrement()` 各自独立加锁
   - 如果连续调用 `increment()` 两次，中间可能被另一个线程插入
   - `counter.increment(); counter.increment();` → 不保证结果为 `+2`
   - 解决方案: 如果需要原子复合操作，要么上层再加锁，要么提供批量接口

5. **未使用的 include 和 using**
   - `#include <atomic>`、`<thread>` 在类代码中未用（测试中用了，应当保留）
   - `using namespace std::chrono_literals` 完全未用
   - 面试印象: 干净的 include 体现工程素养

### 改进后的类声明

```cpp
class Counter
{
public:
    Counter() = default;

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
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

private:
    long               count{0};
    mutable std::mutex mtx;
};
```

---

## 4. Day03 — ThreadSafeQueue

### 文件: `week1/day03_thread_safe_queue.cpp`

### 🔴 关键 Bug 1: `try_pop` 没有真正 pop

```cpp
// 修复前的 try_pop (ThreadSafeQueueFix)
std::optional<T> try_pop()
{
    std::lock_guard<std::mutex> l(mtx);
    if (que_.empty()) {
        return std::nullopt;
    }
    int val = que_.front();          // ← 只读取不删除
    return std::optional<T>{val};    // ← 忘记调用 que_.pop() !
}
```

**影响:** `try_pop` 返回队首元素的值，但队列长度不变。每次调用返回的都是同一个元素。所有基于 `try_pop` 的测试实际上在测试一个永远不会空的队列，测试结果完全无效。

### 🔴 关键 Bug 2: 硬编码类型

```cpp
int val = que_.front();  // 应为 T val = ...;
```

模板参数 `T` 完全被忽略，类设计失去了泛型意义。

### 🟡 关键 Bug 3: `empty()` / `front()` 未加锁 (ThreadSafeQueue 原始版)

```cpp
T front() const { return m_que.front(); }   // 无锁 → data race
bool empty() const { return m_que.empty(); } // 无锁 → data race
```

TSan 运行时会直接报告 data race。这导致了 TOCTOU 问题。

### TOCTOU 竞态详解

| 时间 | 线程 A (消费者) | 线程 B (另一个消费者) |
|------|----------------|---------------------|
| T1 | `if (!que.empty())` → true | — |
| T2 | — | `que.pop()` 清空队列 |
| T3 | `que.front()` → ❌ 未定义行为 | — |

**修复思路:** 将"判空 + 取值 + 删除"合并为一个原子操作 —— `try_pop()` 内部一次性加锁完成所有操作。

### 线程安全队列接口设计原则

| 版本 | 接口设计 | 是否线程安全 | 问题 |
|------|---------|------------|------|
| 原始版 | `empty()` + `front()` + `pop()` 分开 | ❌ | TOCTOU |
| 修正版 | `try_pop()` 返回 `optional<T>` | ✅ | 原子操作，无外部检查 |
| 进阶版 | `try_pop()` + `wait_and_pop()` | ✅ | 支持阻塞等待 |

### `std::optional<T>` 的设计优势

- C++17 引入，明确语义：`nullopt` = 空，`has_value()` = 有值
- 对比 `pair<bool, T>`：空时不构造 T，不需要默认构造函数
- 对比 `bool try_pop(T& out)` 输出参数: 语义更清晰，调用方只需一行 `auto val = que.try_pop()`
- 对比抛异常: 控制流清晰，无异常开销

---

## 5. Day04 — wait_and_pop + 生产者-消费者

### 文件: `week1/day04_thread_safe_queue_test.cpp`

### 🔴 关键 Bug: `wait_and_pop` 中 `front` 缺少括号

```cpp
void wait_and_pop(T& val)
{
    std::unique_lock<std::mutex> lock(mtx);
    cv_.wait(lock, [this] { return !m_que.empty(); });
    val = m_que.front;   // ← 应为 m_que.front()
    m_que.pop();
}
```

**为什么能编译通过？** —— 模板的惰性实例化（Lazy Instantiation）

C++ 标准规定：类模板的成员函数只有在被 **ODR-used**（被真正调用或取地址）时才会实例化。由于 `main()` 中从未调用 `wait_and_pop`，编译器从未检查该函数的函数体，因此错误被隐藏。

这是一个非常经典的 C++ 陷阱，面试中常被问到。

**验证:** 显式实例化 `ThreadSafeQueue<int>` 并调用 `wait_and_pop` 会触发编译错误:
```
cannot resolve overloaded function 'front' based on conversion to type 'int'
```

### 🟡 `empty()` 未加锁

```cpp
bool empty() const { return m_que.empty(); }  // 无锁 → data race
```

消费者中做 `if (!que.empty())` 然后 `try_pop()`，虽然 `try_pop` 内部加锁，但 `empty()` 本身与 `push`/`pop` 有数据竞争。

**注意:** Day04 中 `try_pop` 已经是原子操作，调用方其实**不需要**先检查 `empty()`。直接调用 `try_pop()` 然后判断返回值即可。

### 条件变量知识点

```cpp
// 标准用法
std::unique_lock<std::mutex> lock(mtx);
cv_.wait(lock, [this] { return !m_que.empty(); });
```

1. **为什么需要 `unique_lock` 而不是 `lock_guard`？**
   - `wait()` 内部需要: unlock → 等待 → re-lock
   - `lock_guard` 不提供 `unlock()` 接口
   - `unique_lock` 提供 `lock()`/`unlock()` 接口

2. **虚唤醒（Spurious Wakeup）**
   - 即使没有 `notify_one/notify_all`，`wait` 也可能返回
   - 使用带 predicate 的重载: `wait(lock, predicate)` 等价于 `while (!predicate()) wait(lock);`
   - predicate 在锁保护下执行，是安全的

3. **`notify_one` vs `notify_all`**
   - `notify_one`: 唤醒一个等待线程，高效
   - `notify_all`: 唤醒所有等待线程，用在"关闭/停止"广播场景

### 生产者-消费者的退出策略

| 方案 | 优点 | 缺点 |
|------|------|------|
| **哨兵值** (sentinel) | 简单、保证消费者看到所有数据 | 需预留特殊值，多消费者需多个 sentinel |
| **`stop()` + `notify_all()`** | 无需预留值 | 需在 `wait` predicate 中检查 stop |
| **`try_pop()` + 超时** | 灵活 | 有延迟、busy-wait 倾向 |

--- 

## 6. 共性问题总结

### 代码风格相关

| 问题 | 说明 | 建议 |
|------|------|------|
| `std::endl` | 非交互场景不必要 flush | 改用 `'\n'` |
| 未使用的 include/using | 降低可维护性 | 定期清理 |
| 注释中英文混杂 | 不一致 | 统一使用中文或英文 |

### 工程习惯相关

| 问题 | 说明 | 建议 |
|------|------|------|
| 模板类型硬编码 | `int val` 应为 `T val` | 模板代码不应出现具体类型 |
| 拷贝操作未显式声明 | 含 mutex 的类拷贝行为隐式删除 | 显式 `= delete` |
| 测试可能掩盖问题 | 测试在错误的假设下运行 | 仔细验证测试逻辑 |

### 面试中的"扣分点"

1. **有编译错误的代码** — `m_que.front` 缺括号，虽然被惰性实例化掩盖，但一旦暴露印象分会大降
2. **逻辑 bug** — `try_pop` 没有 `pop()`，测试结果误导
3. **模板写出具体类型** — 面试官会觉得你模板基础不牢
4. **调用方直接操作锁** — 好在代码中做到了"调用方无直接加锁/解锁" ✅

---

## 7. 面试官追问题库

以下问题是根据第一周代码可以自然延伸的面试题:

### C++ 基础

**Q1: `++` 操作不是原子的，那如何让 `increment` 真正线程安全？**

A: 三种方式：
1. 互斥锁 (`std::mutex` + `lock_guard`) —— 通用，但有锁开销
2. 原子操作 (`std::atomic<long>::fetch_add`) —— 无锁，适合简单类型
3. 不加锁、不原子，通过设计保证（如单线程使用）

**Q2: 说说 `std::lock_guard` 和 `std::unique_lock` 的区别，什么时候用哪个？**

A: `lock_guard` 构造加锁析构解锁，无额外开销，不需要解锁的场景用。`unique_lock` 可以提前 `unlock()`、可以转移所有权、可与 `condition_variable` 配合，但体积更大、略有开销。需要这些特性时用 `unique_lock`，否则用 `lock_guard`。

**Q3: `mutable` 关键字还能用在哪些场景？**

A: 常见场景：
- 互斥锁 (`mutable std::mutex`)
- 缓存 (memoization): `mutable std::map<Key, Value> cache`
- 引用计数: `mutable int ref_count`
- 调试日志: `mutable int access_count`

### 并发

**Q4: 什么是虚唤醒（spurious wakeup）？如何正确处理？**

A: 即使没有 `notify_*` 调用，`wait` 也可能返回。正确做法是使用带 predicate 的重载：
```cpp
cv_.wait(lock, [this] { return !m_que.empty(); });
```
等价于：
```cpp
while (!m_que.empty()) {
    cv_.wait(lock);
}
```

**Q5: 条件变量的 wait 为什么要传 `unique_lock` 而不是已经锁住的 `lock_guard`？**

A: `wait()` 内部需要：
1. unlock 让其他线程可以获取锁并修改条件
2. 阻塞等待 notify
3. 被唤醒后 re-lock 再检查条件
`lock_guard` 不提供 `unlock()`，无法实现第一步。

**Q6: 这个队列是无界的，生产速度 > 消费速度会怎样？如何解决？**

A: 队列无限增长，最终 OOM。解决方案：
1. 有界队列：满时生产者 `wait` 等待消费者消费
2. 丢弃策略：满时丢弃旧数据或新数据
3. 背压机制：通知生产者减速

### 模板

**Q7: 模板的惰性实例化（lazy instantiation）是什么？**

A: 类模板的成员函数只有当被 ODR-used 时才会被编译器实例化。这意味者：
- 即使某个成员函数有编译错误，只要不调用它就能通过编译
- 可以将模板的接口声明与实现分离（只要不使用某些成员）
- 这也是为什么模板代码的编译错误经常在使用时而不是定义时出现

**Q8: C++20 的 `requires` / C++17 的 `if constexpr` 对模板编程有什么改进？**

A:
- `if constexpr`: 编译期分支，消除 SFINAE 的复杂技巧
- `requires`: 显式表达模板约束，编译错误信息更清晰
- 例: `requires std::is_default_constructible_v<T>` 要求 T 可默认构造

### 设计

**Q9: 设计线程安全队列时，`empty()` 接口有意义吗？**

A: 基本没有。因为调用方获得 `empty()` 结果时，其他线程可能已经改变了队列状态（TOCTOU）。调用方应该直接调用 `try_pop()` 或者 `wait_and_pop()`，不需要先检查 `empty()`。`empty()` 仅在测试或单线程调试中有意义。

**Q10: `try_pop` 返回 `std::optional<T>` 相比 `bool try_pop(T& out)` 有什么优势？**

A:
1. 调用方接口清晰：`auto val = que.try_pop()`
2. 不需要默认构造函数：`std::optional` 可以持有非默认构造的类型
3. 值语义：无需引用传递，无需担心 dangling reference
4. 组合性：可以和 `std::nullopt` 比较，支持 monadic 操作（C++23）

---

## 8. 修复记录

| 文件 | 修复内容 | 是否破坏接口 |
|------|---------|------------|
| `day01_counter.cpp` | `std::endl` → `'\n'`，去除未使用参数 `argc/argv` | 否 |
| `day02_counter_mutex.cpp` | 显式 `= delete` 拷贝/移动操作，添加文档注释 | 否 |
| `day03_thread_safe_queue.cpp` | `ThreadSafeQueueFix::try_pop` 添加 `que_.pop()`，`int val` → `T val`，添加注释 | 否 |
| `day04_thread_safe_queue_test.cpp` | `wait_and_pop` 中 `m_que.front` → `m_que.front()`，`empty()` 加锁，main 重写为 sentinel 退出 | 否 (wait_and_pop 之前从未被调用) |

---

> **文档版本**: v1.0
> **适用周次**: 第 1 周 (基础封装)
> **审查时间**: 2026-04-27

# Code Review: Week 2 Day 04 — shared_ptr (引用计数)

**评审日期**: 2026-04-28
**评审人**: Claude (Senior C++ Engineer)

---

## 整体评价

该 shared_ptr 实现存在**多处严重的内存错误**，在启用 ThreadSanitizer 后运行时检测到多次 heap-use-after-free。10 个 GTest "PASS" 中有多处是"碰巧不崩溃"的 UB，且堆破坏已导致后续完全不相关的测试（dereference）失败并最终 SEGV 崩溃。这是典型"测试全绿但代码有严重 bug"的反面教材。下文逐项分析。

---

## 逐项评审

### 1. [严重] 移动构造函数：source 残留 ref_count 导致 double-free

```cpp
shared_ptr(shared_ptr&& other) noexcept
{
    std::swap(ptr_, other.ptr_);  // 见问题 2
    other.ptr_ = nullptr;
    ref_count_ = other.ref_count_;  // ← BUG: 未置空 other.ref_count_
}
```

**问题**: 移动后 `other.ref_count_` **仍指向原来的引用计数对象**，没有被置空。当两个 shared_ptr 析构时：

1. `this` 析构 → `desconstruct()` 将计数减到 0 → `delete ref_count_`
2. `other` 析构 → `desconstruct()` 读 `*ref_count_` → **use-after-free！**

**ThreadSanitizer 确认**:
```
WARNING: ThreadSanitizer: heap-use-after-free
  #0 shared_ptr<Tracker>::~shared_ptr()
  #1 Day04_move_construct_Test::TestBody()
  Previous write: operator delete → shared_ptr<Tracker>::desconstruct()
```

**正确实现**:
```cpp
shared_ptr(shared_ptr&& other) noexcept
    : ptr_(other.ptr_), ref_count_(other.ref_count_)
{
    other.ptr_ = nullptr;
    other.ref_count_ = nullptr;  // ← 必须置空！
}
```
或使用 `std::exchange`:
```cpp
shared_ptr(shared_ptr&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr))
    , ref_count_(std::exchange(other.ref_count_, nullptr))
{}
```

---

### 2. [严重] 移动构造函数：读取未初始化的 ptr_

```cpp
std::swap(ptr_, other.ptr_);  // ptr_ 未初始化 → UB
```

**问题**: 与 Day03 的 unique_ptr 相同的问题。`ptr_` 是原始指针成员，没有类内初始值，构造函数也没有初始化列表。进入构造函数体时 `ptr_` 的值是垃圾值，`std::swap` 读取了未初始化的内存。

**后果**: 此处"碰巧正确"是因为 swap 将垃圾值 swap 到 `other.ptr_`，然后立即被 `other.ptr_ = nullptr` 覆盖。但读取未初始化内存本身就是 UB，抹消了编译器优化的一切保证。

---

### 3. [严重] 拷贝赋值和移动赋值：自赋值导致 double-free

```cpp
shared_ptr& operator=(const shared_ptr& other) noexcept
{
    desconstruct();          // ← 如果 this == &other，先 delete 了自己
    ptr_       = other.ptr_;
    ref_count_ = other.ref_count_;
    (*ref_count_)++;         // ← 写入已释放的内存
    return *this;
}
```

**问题**: 当 `sp = sp`（自赋值）且 `use_count() == 1` 时：
1. `desconstruct()` 认为自己是最后一个引用 → delete `ptr_` + delete `ref_count_`
2. `ptr_ = other.ptr_` → 写已释放的指针（UB）
3. `ref_count_ = other.ref_count_` → 读已释放的指针（UB）
4. `(*ref_count_)++` → 写已释放的内存（UB → 潜在的 double-free）

**修复**: 在最前面加自赋值检查:
```cpp
if (this == &other) return *this;
```

**移动赋值**有同样的自赋值问题。

---

### 4. [严重] 默认构造的 shared_ptr 拷贝后 double-free

```cpp
shared_ptr() noexcept : ptr_(nullptr)
{
    ref_count_  = new int();
    *ref_count_ = 0;       // ref_count 分配了但值为 0
}
```

**问题**: 拷贝一个默认构造的 shared_ptr：
```cpp
shared_ptr<int> sp1;       // ptr_=null, *ref_count_=0
shared_ptr<int> sp2(sp1);  // ptr_=null, ref_count_=同一块, (*ref_count_)++ → 1
// sp2 析构: desconstruct() → *ref_count_ == 1 → delete ref_count_
// sp1 析构: desconstruct() → *ref_count_ 读取已释放内存 → UB!
```

**根因**: 空 shared_ptr 不应分配 ref_count（`std::shared_ptr` 在空状态直接设 `ref_count_ = nullptr`）。拷贝空 shared_ptr 是合法操作，必须正确处理。

**修复**: 空 shared_ptr 应将 `ref_count_` 设为 `nullptr`，拷贝时处理空状态:
```cpp
shared_ptr() noexcept : ptr_(nullptr), ref_count_(nullptr) {}
```

拷贝构造需要检查 `ref_count_` 是否为空:
```cpp
shared_ptr(const shared_ptr& other) noexcept
    : ptr_(other.ptr_), ref_count_(other.ref_count_)
{
    if (ref_count_) ++(*ref_count_);
}
```

但这样设计更复杂，因为 `desconstruct()` 也需要处理 `ref_count_ == nullptr`。更简单的方案是确保空 shared_ptr 要么永远不被拷贝，要么给空状态一个有效的 ref_count 体系。最佳实践是**根本不为空状态分配 ref_count**，用 `nullptr` 表示空，然后在所有操作中加判空。

---

### 5. [中等] `desconstruct()` 的语义和命名问题

```cpp
void desconstruct()  // typo: 应为 dec_ref / release / destroy
{
    if (*ref_count_ > 1) {
        (*ref_count_)--;
    } else {
        if (ptr_) delete ptr_;
        if (ref_count_) delete ref_count_;
    }
}
```

**命名**: `desconstruct` 应为 `dec_ref` 或 `release_ref`。当前拼写可能是 `deconstruct` 的笔误。在 C++ 中 `destroy` 或 `release` 更常见。

**逻辑**: 当 `*ref_count_ == 1` 时，说明这是最后一个所有者，同时 delete 资源和控制块。正确。但当 `*ref_count_ == 0`（来自默认构造的拷贝）时，误入 else 分支，删除 ref_count_ 导致后续 double-free。

**当 `ref_count_` 可能为 `nullptr` 时**: 当前代码没有处理这种状态，如果改成 `nullptr` 方案需要加保护。

---

### 6. [中等] `operator*()` 的副作用

```cpp
T& operator*() const noexcept
{
    std::cout << "val: " << *ptr_ << std::endl;  // ← 不应该有！
    return *ptr_;
}
```

**问题**: `operator*` 应该是一个纯粹的、无副作用的解引用操作。打印到 stdout 违反了**最小惊讶原则**——使用 `*sp` 的调用方不会预期有控制台输出。

而且由于堆破坏，这里打印的值是 `1` 而不是 `42`，导致测试失败。

---

### 7. [中等] 析构函数中的调试打印

```cpp
~shared_ptr()
{
    std::cout << "~shared_ptr ref_count_: " << *ref_count_ << std::endl;
    desconstruct();
}
```

同样是调试残留。生产代码中析构函数不应有输出。在单元测试中，这些输出也严重干扰了测试结果的可读性。

---

### 8. [低] 头文件包含问题

```cpp
#include <algorithm>   // 未直接使用
#include <cstddef>     // 未直接使用 (size_t 由 <algorithm> 间接提供)
#include <iostream>    // 仅用于调试打印，不应在头文件中使用
```

- 应移除 `<iostream>`（引入全局流对象）
- 如使用 `std::swap` 应显式包含 `<utility>`（不是 `<algorithm>`）
- 如使用 `size_t` 应包含 `<cstddef>`（实际上已包含，但 `use_count()` 中的 `size_t` 正确）

---

### 9. [低] 测试中的调试输出

测试文件中散布了大量 `std::cout` 调试打印：
```
~shared_ptr ref_count_: 3
Tracker construct
Tracker alive: 2
after create sp1 sp2
...
```

这些输出使测试结果难以阅读，应在最终版本中移除。

---

## 测试结果分析

| 测试 | 结果 | 实际状态 |
|------|------|----------|
| default_construct | PASS | 分配了不必要的 ref_count |
| ptr_construct | PASS | 正常 |
| copy_construct_increases_count | PASS | 正常 |
| last_reference_destroys | PASS | 正常 |
| copy_assignment | PASS | 自赋值未测 |
| move_construct | PASS* | **heap-use-after-free！** TSAN 报警 |
| move_assign | PASS* | **heap-use-after-free！** TSAN 报警 |
| dereference | **FAIL** | 1 != 42 — 堆破坏导致的值错误 |
| use_count_precision | PASS | 正常 |
| bool_conversion | PASS* | 默认构造的拷贝引起 double-free |
| shared_ownership | PASS | 正常 |

**进程最终 SEGV** — 由堆破坏（double-free）导致。

---

## 面试扩展话题

### 1. "shared_ptr 的引用计数为什么不能在栈上？"

不同 shared_ptr 实例各自是一个独立的对象，栈上的成员变量无法在实例间共享。必须将计数分配到堆上，让所有实例共享同一个控制块。

### 2. "移动 shared_ptr 为什么不需要原子操作？"

移动操作转移的是所有权，只修改源和目标两个 shared_ptr 对象。这两个对象由同一个线程操作（移动操作是同步调用），不存在竞争。引用计数的原子操作只在**拷贝**时必要，因为多个线程可能同时拷贝同一个 shared_ptr。

### 3. "make_shared 相比 new + shared_ptr 构造的优势？"

```cpp
auto sp = std::make_shared<T>(args...);  // 一次分配: 对象 + 控制块
shared_ptr<T> sp(new T(args...));         // 两次分配: 对象 + 控制块
```

`make_shared` 将对象和控制块分配在同一块内存中，减少一次堆分配，提高缓存局部性。代价是对象延迟释放——因为控制块中有 weak_ptr 计数，即使 shared_ptr 全部销毁，对象也要等到 weak_ptr 也清零才释放。

### 4. "enable_shared_from_this 的原理？"

在对象内部隐藏一个 `weak_ptr<this>`，通过 `shared_from_this()` 返回一个新的 shared_ptr。适用于回调或异步场景中需要将 this 提升为 shared_ptr 时。

### 5. "如何检测和避免循环引用？"

A 持有 B 的 shared_ptr，B 持有 A 的 shared_ptr → 两者永远无法释放。用 `weak_ptr` 打破循环。常见的场景是 parent → child 用 shared_ptr，child → parent 用 weak_ptr。

---

## 总结

| 项目 | 评价 |
|------|------|
| 正确性 | **严重缺陷**：3 处 heap-use-after-free，堆破坏导致 SEGV |
| 引用计数设计 | 正确思路（堆上分配），但空状态处理和移动语义有严重疏漏 |
| 移动语义 | 忘记置空 `other.ref_count_`，导致 double-free |
| 自赋值安全 | 未被考虑，拷贝/移动赋值都是 double-free |
| 代码风格 | 大量调试打印残留，`desconstruct` 拼写错误 |
| 测试质量 | 测试通过了"碰巧没挂"的场景，但 UB 已被 TSAN 和 SEGV 证实 |

**总体评价**: 对 shared_ptr 的引用计数原理有基本理解，但实现中存在多处严重内存安全问题。最核心的两个问题：(1) 移动构造函数没有将 `other.ref_count_` 置空；(2) 自赋值未处理。虽然 GTest 报告 10/11 通过，但堆破坏掩盖了真实情况——启用 AddressSanitizer/ThreadSanitizer 后立即暴露了问题。这些 bug 说明在**资源所有权转移的精确性**上需要进一步打磨，以及对 **C++ 对象生命周期**的严谨处理。

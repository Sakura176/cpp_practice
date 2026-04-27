# Code Review: Week 2 Day 02 — ScopedMutex (RAII 模板)

**评审日期**: 2026-04-27
**评审人**: Claude (Senior C++ Engineer)

---

## 整体评价

代码质量优秀，RAII 核心语义正确，测试覆盖全面。作为一个模拟 `std::lock_guard` 的教学练习，在功能正确性、模板泛化能力和异常安全方面都达到了生产级认知水平。唯一值得深入讨论的是设计上的"有意识简化"取舍，下文逐项展开。

---

## 逐项评审

### 1. 模板接口：BasicLockable 泛化

```cpp
template<typename Mutex>
class ScopedMutex {
    explicit ScopedMutex(Mutex& mtx);
    ~ScopedMutex();
};
```

模板参数命名 `Mutex`，但实际要求的是满足 BasicLockable 概念（有 `lock()` / `unlock()` 成员）的**任意类型**。代码正确泛化到了 `std::timed_mutex`（测试 3 验证），也支持 `TrackMutex` 这样的测试替身。

**面试追问**: "如果传入一个没有 `lock()`/`unlock()` 的类型，编译器错误信息可读性如何？"
- 当前实现在实例化时才会报错，错误信息会层层展开模板内部，对初学者不友好
- C++20 Concept 可以给出清晰的静态断言消息：`template<BasicLockable Mutex>`；C++17 可以用 `SFINAE` 或 `static_assert` 改善

**结论**: 教学练习层面完全可接受。高级话题：C++20 Concepts 能显著改善此类模板的 DX。

---

### 2. 引用成员 vs 指针成员

```cpp
private:
    Mutex& mtx_; // 引用
```

选择引用是正确的：
- **无空状态**: 引用不能为 null，静态保证 `mtx_` 一定引用有效对象（生命周期由调用方保证，同 `std::lock_guard`）
- **赋值被删除**: 引用成员导致编译器隐式删除赋值操作符，与本类 `= delete` 拷贝/移动的目标一致——"不可赋值"对锁包装是合理的语义

**折衷**: 引用成员使得该类天然不可重新绑定。如果将来需要支持移动所有权（类似 `std::unique_lock`），必须改为指针。根据设计注释，当前定位就是最轻量的包装，不打算支持移动，所以引用是最优选择。

---

### 3. 析构函数与异常安全

```cpp
~ScopedMutex() { mtx_.unlock(); }
```

`unlock()` 不抛出异常是 `std::mutex` 族系的约定，`TrackMutex::unlock()` 也不抛。但在泛型上下文中，如果用户传入一个 `unlock()` 会抛异常的自定义 BasicLockable，析构函数中抛出的异常会导致 `std::terminate`。

**实际影响**: 几乎为零——标准库所有 mutex 的 `unlock()` 都是 noexcept。教学代码无需处理此边缘情况。

**异常安全测试**: 测试 4 非常好——通过 `throw` + `catch(...)` 验证栈展开时析构函数正确调用 `unlock()`。这是 RAII 的核心卖点，值得单独测试。

---

### 4. 禁止拷贝（= delete）

三个特殊成员函数全部正确管理：

| 函数 | 状态 | 理由 |
|------|------|------|
| 构造函数 | 默认 | 加锁 |
| 析构函数 | 默认 | 解锁 |
| 拷贝构造 | `= delete` | 锁不可复制（两个对象持同一把锁？） |
| 拷贝赋值 | `= delete` | 同上 |
| 移动构造 | **未声明**（隐式 delete） | 设计决定：不允许所有权转移 |
| 移动赋值 | **未声明**（隐式 delete） | 同上 |

**设计讨论**: `std::lock_guard` 同样禁止拷贝/移动。`std::unique_lock` 支持移动，代价是多了 bool 成员跟踪锁状态。当前设计选择了最小开销路径，与注释一致。

**测试 6（`no_copy`）** 在编译期通过注释演示了拷贝构造被禁止，但未用静态断言验证。可以用 `static_assert(!std::is_copy_constructible_v<ScopedMutex<std::mutex>>)` 来增强。

---

### 5. 测试质量（总体优秀）

| 测试 | 覆盖维度 | 评价 |
|------|----------|------|
| lock_unlock_raii | 基本 RAII 语义 | 好，TrackMutex 巧妙 |
| with_std_mutex | 标准库集成 | 好 |
| with_timed_mutex | 模板泛化验证 | 好，`try_lock()` 间接验证解锁 |
| exception_safety | 栈展开 | 好，RAII 核心用例 |
| mutual_exclusion | 并发互斥 | **非常好**，2 线程 × 10000 次递增 |
| no_copy | 接口限制 | 仅注释演示，可增加 `static_assert` |

**特别值得表扬**: `TrackMutex` 辅助类型是一个很好的测试技巧——通过计数的 lock/unlock 观察 RAII 行为，无需真正加锁的开销和死锁风险。`mutual_exclusion` 测试用 10000 次迭代做压力测试，验证了 ScopedMutex 在多线程下确实保证了互斥访问，这个测试很有说服力。

**可补充的测试**:
1. `static_assert(!std::is_copy_constructible_v<...>)` 编译期验证拷贝禁止
2. 与 `std::recursive_mutex` 配合的测试（递归锁也是 BasicLockable）
3. `std::shared_mutex`（C++17，兼具 BasicLockable 和 SharedBasicLockable）

---

### 6. 工程细节

- **.cpp 文件**: 完全符合模板类的惯例——声明为空，注释说明模板实现在头文件中。干净。
- **include guard**: 使用传统 `#ifndef` 风格，与项目一致。无 `#pragma once` 的移植性问题。
- **C++17**: CMake 配置为 C++17，测试中使用了 `constexpr` 变量，合理。

---

## 面试扩展话题

如果这是面试中的代码，可以追问：

1. **"如果需要在已锁的 mutex 上构造 ScopedMutex（即 adopt_lock 语义），如何实现？"**
   - 参考 `std::lock_guard` 的 `adopt_lock_t` 标签：加一个重载构造函数 `ScopedMutex(Mutex& mtx, std::adopt_lock_t)`，不调用 `mtx.lock()`。

2. **"ScopedMutex<std::mutex> 和 std::lock_guard<std::mutex> 在汇编层面有区别吗？"**
   - 没有本质区别。两者都是 RAII 包装，编译器应生成相同或近似的代码。

3. **"模板实现全在头文件中。如果希望隐藏实现，怎么做？"**
   - 可以使用显式实例化（`template class ScopedMutex<std::mutex>` 放在 .cpp 中），头文件只保留声明。代价是限制了可实例化的类型集合。

---

## 总结

| 项目 | 评价 |
|------|------|
| 正确性 | 全部 6 个测试通过，RAII 语义正确 |
| 模板设计 | BasicLockable 泛化正确，无过度设计 |
| 异常安全 | 栈展开时锁正确释放，测试覆盖 |
| 测试质量 | 优秀——TrackMutex + 并发测试 + 异常安全 |
| 代码风格 | 简洁、克制，注释清晰 |
| 工程意识 | 引用成员、拷贝禁止等设计决策有明确理由 |

**总体评价**: 这是高质量的教学代码。ScopedMutex 作为 `std::lock_guard` 的简化实现，在正确性、安全性和可读性上都做到了应有水准。测试覆盖了核心路径和边界情况，尤其并发测试和异常安全测试体现了对 RAII 本质的深入理解。如果希望在工程能力上更进一步，可以考虑 C++20 Concepts 改进模板接口的约束检查。

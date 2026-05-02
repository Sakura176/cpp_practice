# Code Review: Week 2 Day 03 — unique_ptr (RAII 独占所有权)

**评审日期**: 2026-04-28
**评审人**: Claude (Senior C++ Engineer)

---

## 整体评价

代码基本功能可以工作，测试全部通过，RAII 核心思路正确。但**存在一个严重的未定义行为（UB）bug**（移动构造函数中读未初始化的成员变量），以及多处值得商榷的实现选择。作为教学练习，这些问题是极好的学习素材，下文逐项深入分析。

---

## 逐项评审

### 1. [严重] 移动构造函数：未定义行为

```cpp
unique_ptr(unique_ptr&& other) noexcept
{
    std::swap(ptr_, other.ptr_);  // ← BUG: ptr_ 未初始化!
    if (!other) {                 // ← 读取未初始化内存 → UB
        delete other.ptr_;        // ← 可能 delete 野指针 → UB
    }
    other.ptr_ = nullptr;
}
```

**问题**: `ptr_` 是原始指针成员，没有类内初始值（`T* ptr_;`），构造函数也没有使用初始化列表。进入构造函数体时 `ptr_` 的值是**未初始化的垃圾值**。`std::swap` 读取了这个垃圾值写入 `other.ptr_`，然后 `if (!other)` 和后续的 `delete` 操作都可能操作垃圾值。

**这是确定的未定义行为** — 在大多数实现上可能"碰巧不崩溃"（因为未初始化的栈变量恰好为 0，或 TriviallyConstructible 默认不初始化），但 C++ 标准明确禁止。

**正确实现**:
```cpp
unique_ptr(unique_ptr&& other) noexcept : ptr_(other.release()) {}
```
或初始化列表方式：
```cpp
unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_)
{
    other.ptr_ = nullptr;
}
```

**面试追问**: *"未初始化 POD 成员的值是什么？为什么说读取它是 UB？"*
- 关键区分：全局/静态变量会零初始化，但**非静态成员**在默认构造的函数体中仍然未初始化
- 标准说读取未初始化对象是 UB，不是"不确定值"那么简单。UB 意味着编译器可以生成任意代码

---

### 2. [正确性/风格] 裸指针构造函数：迷惑的空语句

```cpp
explicit unique_ptr(T* ptr) noexcept
{
    ptr_ = ptr;
    ptr  = nullptr;  // ← 置空的是参数 ptr，不是成员 ptr_
}
```

**问题**: `ptr = nullptr;` 将**函数参数**（局部变量）置空，对调用方完全无影响。这行代码纯属冗余，推测是编写者意图置空成员时写错了变量名，或是混淆了参数与成员。

**后果**: 没有运行时错误（参数确实被置空了，但参数马上随函数结束而销毁），但代码含义错误，反映对参数作用域的理解不清晰。

**改进**: 使用初始化列表，根本不存在这种混淆：
```cpp
explicit unique_ptr(T* ptr) noexcept : ptr_(ptr) {}
```

---

### 3. 移动赋值：功能正确但模式不典型

```cpp
unique_ptr& operator=(unique_ptr&& other) noexcept
{
    std::swap(ptr_, other.ptr_);
    if (other) {
        delete other.ptr_;
    }
    other.ptr_ = nullptr;
    return *this;
}
```

**分析**: 这段代码的思路是通过 `swap` 交换当前指针和目标指针，然后让 `other`（现在持有原 `this` 的指针）析构掉它。这个模式在功能上是正确的：
- `this` 获得 `other` 的资源
- `other` 获得 `this` 的旧资源并被 `delete` + 置空

**问题**: 这种实现过于绕——别人读代码需要停下来推理 `swap` 和后续 `delete` 的关系。标准库 `std::unique_ptr` 的实现方式更直接：

```cpp
// 方式 A: 复用 reset + release
reset(other.release());
return *this;
```

```cpp
// 方式 B: 手动管理
delete ptr_;
ptr_ = other.ptr_;
other.ptr_ = nullptr;
return *this;
```

**自赋值情形**: 自移动时 `swap` 是空操作，然后检查 `if (other)` → `true`（如果当前持有资源）→ delete 自己的资源 → 置空。结果是**资源被删除，指针被置空**，这符合"自移动后对象处于有效但未指定状态"的要求，但会造成意外的资源释放，不如提前检查 `if (this != &other)` 来得清晰。

---

### 4. 头文件依赖问题

```cpp
#include <iostream>     // 未使用，应移除
#include <utility>       // 缺少！std::swap 需要此头文件
```

- `<iostream>` 引入了全局流对象，在头文件中包含是**不经济的**——每个包含此头文件的翻译单元都会额外引入流对象的定义
- `std::swap` 在标准 `<utility>` 中声明。当前代码依赖 `<iostream>` 内部间接包含 `<utility>`，这是**未指定行为**——不同标准库实现可能不同

---

### 5. 缺少类内成员初始值

```cpp
private:
    T* ptr_;  // ← 无默认值
```

**问题**: 没有给 `ptr_` 默认值 `nullptr`。虽然当前所有构造函数都正确初始化了 `ptr_`，但如果将来有人添加一个新构造函数而忘记初始化，会再次触发 UB。

**改进**: C++11 类内初始值：
```cpp
private:
    T* ptr_ = nullptr;
```

这样做会**自动**让默认构造函数体为空：
```cpp
unique_ptr() noexcept = default;  // ptr_ 自动初始化为 nullptr
```

---

### 6. 值得肯定的设计决策

| 设计 | 评价 |
|------|------|
| `explicit` 单参构造函数 | 防止隐式转换，正确 |
| `explicit operator bool()` | 防止意外布尔转换（如 `p + 1`），正确 |
| `noexcept` 标记 | 所有函数均正确标记 |
| 拷贝操作 `= delete` | 独占所有权语义正确 |
| `release()` / `reset()` 接口 | 语义与 `std::unique_ptr` 一致 |

---

## 测试质量评估

| 测试 | 覆盖维度 | 评价 |
|------|----------|------|
| default_construct | 默认构造为 null | 好 |
| ownership_and_destruction | RAII 析构释放 | 好，Tracker 辅助类巧妙 |
| move_constructor | 移动构造转移所有权 | 基本验证，但能测出已实现的 UB |
| move_assignment | 移动赋值 | 好 |
| dereference | operator\* / operator-> | 好 |
| release | 释放所有权 | 好，验证释放后不 delete |
| reset | 重置指针 | 好 |
| move_into_container | 容器兼容性 | 好，验证移动语义与标准容器兼容 |
| no_copy | 拷贝禁止 | 仅注释，可增加 `static_assert` |

**建议补充**:
1. `static_assert(!std::is_copy_constructible_v<unique_ptr<int>>)` 编译期验证
2. 自赋值移动测试（边界情况验证）
3. 在空 `unique_ptr` 上调用 reset(new T) （空指针的 delete 是安全的，应该测）

---

## 面试扩展话题

### 1. "实现一个 `make_unique<T>(args...)`"

这是 C++14 引入的工具函数，本质是参数转发 + 异常安全：
```cpp
template<typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```
C++17 起 `std::make_unique` 是标准库的一部分。

### 2. "为什么要禁止拷贝？如何绕过？"

两个 `unique_ptr` 指向同一资源 → 双重 delete。如果需要共享所有权，用 `shared_ptr`（引用计数）。可以显式调用 `get()` 获取裸指针然后 `delete`，但这违背了智能指针的初衷。

### 3. "支持数组特化 `unique_ptr<T[]>`"

标准库提供了数组偏特化，区别是使用 `delete[]` 代替 `delete`，并支持下标访问 `operator[]`。这是一个经典的模板偏特化教学案例。

### 4. "自定义删除器"

`std::unique_ptr<T, Deleter>` 支持自定义删除器（函数指针、lambda、仿函数等），适用于需要 `fclose` 而不是 `delete` 的场景。删除器通常作为模板参数的一部分以零开销抽象。

### 5. "移动后源对象的状态"

标准规定"移动后源对象处于有效但未指定的状态"。对本练习而言就是 `ptr_ = nullptr`。这一点很重要：移动后不应再对源对象做前提条件严格的假设，唯一安全的操作是析构或赋值。

---

## 总结

| 项目 | 评价 |
|------|------|
| 正确性 | **存在 UB**：移动构造函数读取未初始化的成员；其他功能正确 |
| 语义设计 | 拷贝删除、移动转移、explicit bool — 全部正确 |
| 实现质量 | 移动构造函数有严重缺陷；裸指针构造有迷惑代码 |
| 测试质量 | 覆盖全面，Tracker 辅助类设计好，但缺少编译期检查 |
| 代码风格 | 可读，但包含无用头文件和不必要的复杂写法 |
| 工程意识 | 整体框架理解正确，细节实现需要加强 |

**总体评价**: 核心语义理解到位，RAII 和独占所有权的概念正确，测试覆盖了主要路径。但**移动构造函数的 UB bug 是必须修复的硬伤**，反映了对未初始化内存和构造顺序的理解需要加深。此外，参数与成员混淆、多余的 `std::swap` 绕路实现、无用的头文件包含等问题，说明在代码精确性和审慎性上还有提升空间。这些问题一旦指出后很容易理解，属于"知道就能避免"的范畴——这也是 code review 的价值所在。

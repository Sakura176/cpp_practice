#ifndef SHARED_PTR_H
#define SHARED_PTR_H

// NOTE: <iostream> 仅用于调试打印，生产代码应移除（详见评审文档）
// NOTE: <cstddef> 提供 size_t，若改用 unsigned long 可移除
#include <cstddef>
#include <iostream>
// NOTE: 使用 std::swap 应包含 <utility>，<algorithm> 不保证提供 swap
#include <utility>

/**
 * @brief 极简 shared_ptr<T>
 *
 * 要求:
 *  1. 引用计数（堆上分配的整数）
 *  2. 拷贝构造/拷贝赋值: 增加引用计数
 *  3. 析构: 减少引用计数，减到 0 时 delete 资源
 *  4. operator* 和 operator-> 提供指针访问
 *  5. use_count() 返回当前引用计数
 *  6. 支持移动语义（移动不改变引用计数）
 *  7. 可隐式转换为 bool
 *
 * ---- 知识点 ----
 *
 *  引用计数原理:
 *    控制块（Control Block）在堆上分配，包含引用计数字段。
 *    所有 shared_ptr 实例共享这个控制块。
 *    拷贝 → count++，析构 → count--，count==0 → delete 资源 + 控制块。
 *
 *  为什么控制块不能在对象内部?
 *    如果引用计数是 shared_ptr 的成员变量，不同 shared_ptr 实例
 *    各自维护自己的计数，无法同步。所以计数必须在堆上共享。
 *
 *  线程安全:
 *    引用计数的增减必须是原子操作（否则多线程下计数会错乱）。
 *    标准库的 shared_ptr 保证引用计数线程安全，
 *    但指向的对象本身不是线程安全的。
 *    本练习不要求实现原子操作（可以用普通 int 模拟）。
 *
 *  make_shared 的优势:
 *    std::make_shared<T>(args...) 一次分配对象和控制块，
 *    减少一次内存分配，提高缓存局部性。
 *    这个练习不要求实现 make_shared 工厂函数。
 *
 *  weak_ptr:
 *    用于打破循环引用，不增加引用计数。
 *    这个练习不要求实现。
 */

template<typename T>
class shared_ptr
{
public:
    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    /** 默认构造: 空指针 */
    shared_ptr() noexcept;

    /** 从裸指针构造: 分配控制块，引用计数为 1 */
    explicit shared_ptr(T* ptr);

    /** 析构: count--，减到 0 时 delete */
    ~shared_ptr();

    // 拷贝语义: 增加引用计数
    shared_ptr(const shared_ptr& other) noexcept;
    shared_ptr& operator=(const shared_ptr& other) noexcept;

    // 移动语义: 转移所有权，不改变引用计数
    shared_ptr(shared_ptr&& other) noexcept;
    shared_ptr& operator=(shared_ptr&& other) noexcept;

    /** 解引用 */
    T& operator*() const noexcept;
    T* operator->() const noexcept;

    /** 获取裸指针 */
    T* get() const noexcept;

    /** 返回引用计数 */
    size_t use_count() const noexcept;

    /** bool 转换 */
    explicit operator bool() const noexcept;

private:
    // 命名: desconstruct → dec_ref / release_ref 更清晰
    // 【BUG】当 *ref_count_ == 0 时（来自空 shared_ptr 的拷贝）误入 else 分支，delete ref_count_ → 后续 double-free
    // 【注意】当 ref_count_ 为 nullptr 时（如果改用 nullptr 表示空），这里会崩溃
    void dec_ref()
    {
        if (!ref_count_)
            return;

        if (*ref_count_ > 1) {
            (*ref_count_)--;
        } else {
            if (ptr_) {
                delete ptr_;
                ptr_ = nullptr;
            }
            if (ref_count_) {
                delete ref_count_;
                ref_count_ = nullptr;
            }
        }
    }

private:
    T* ptr_;
    // 【设计】ref_count_ 作为裸指针管理堆上的引用计数。
    // 空状态应设为 nullptr。但当前默认构造为它分配了 int(0)，
    // 导致空 shared_ptr 被拷贝时 double-free（参见 desconstruct）
    int* ref_count_;
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================

// 【设计】默认构造：为 nullptr 分配 ref_count 是浪费且危险的。
// 问题: 拷贝一个默认构造的 shared_ptr → 两个实例共享 ref_count → double-free
// 正确做法: ref_count_ = nullptr; 由调用方保证不拷贝空对象，或在所有操作中处理 nullptr
template<typename T>
shared_ptr<T>::shared_ptr() noexcept : ptr_(nullptr),
                                       ref_count_(nullptr)
{
}

template<typename T>
shared_ptr<T>::shared_ptr(T* ptr) : ptr_(ptr)
{
    // std::cout << "shared_ptr val: " << (*ptr) << std::endl;
    ref_count_ = new int(1);
    // *ref_count_ = 1;
}

// 析构函数中的 std::cout 是调试残留，生产代码应移除
// 注意: 如果 ref_count_ 已被其他路径 delete（如移动后 source 未置空），此处读 *ref_count_ 是 use-after-free
template<typename T>
shared_ptr<T>::~shared_ptr()
{
    // std::cout << "~shared_ptr ref_count_: " << *ref_count_ << std::endl;
    dec_ref();
}

// 拷贝构造函数: 引用计数 +1，语义正确。
// 隐患: 当 other 是默认构造的空 shared_ptr（*ref_count_ == 0）时，
// 两个实例共享 ref_count_，析构时为"最后一个"互相争抢 delete → double-free
template<typename T>
shared_ptr<T>::shared_ptr(const shared_ptr& other) noexcept : ptr_(other.ptr_)
{
    ref_count_ = other.ref_count_;
    (*ref_count_)++;
}

// 【BUG】没有自赋值检查: sp = sp 时 use_count() == 1 → desconstruct() delete 自己 → 后续操作已释放内存 → UB
// 修复: if (this == &other) return *this; 放在 desconstruct() 之前
template<typename T>
shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr& other) noexcept
{
    if (this == &other)
        return *this;

    dec_ref();

    ptr_       = other.ptr_;
    ref_count_ = other.ref_count_;
    (*ref_count_)++;

    return *this;
}

// 【BUG 1】ptr_ 未初始化就被 std::swap 读取 → UB（同 day03 问题）
// 【BUG 2】other.ref_count_ 未置空！
// 移动后 this 和 other 共享同一 ref_count_，两者析构时 double-free
// 正确实现:
//   shared_ptr(shared_ptr&& other) noexcept
//       : ptr_(std::exchange(other.ptr_, nullptr))
//       , ref_count_(std::exchange(other.ref_count_, nullptr))
//   {}
template<typename T>
shared_ptr<T>::shared_ptr(shared_ptr&& other) noexcept
    : ptr_(std::exchange(other.ptr_, nullptr)),
      ref_count_(std::exchange(other.ref_count_, nullptr))
{
}

// 【BUG 1】同拷贝赋值: 自赋值时 use_count() == 1 → double-free
// 【BUG 2】other.ref_count_ 未置空 → 移动后 source 与 this 共享 ref_count_，析构时 double-free
// 正确实现:
//   if (this == &other) return *this;
//   reset(); // 释放当前资源
//   ptr_ = std::exchange(other.ptr_, nullptr);
//   ref_count_ = std::exchange(other.ref_count_, nullptr);
template<typename T>
shared_ptr<T>& shared_ptr<T>::operator=(shared_ptr&& other) noexcept
{
    if (this == &other)
        return *this;

    dec_ref();

    ptr_       = std::exchange(other.ptr_, nullptr);
    ref_count_ = std::exchange(other.ref_count_, nullptr);
    return *this;
}

// NOTE: operator* 不应有副作用（打印到 stdout）。
// 调用方期望的是纯粹的解引用操作，控制台输出违反最小惊讶原则。
// 去掉 std::cout 行即可。
template<typename T>
T& shared_ptr<T>::operator*() const noexcept
{
    // std::cout << "val: " << *ptr_ << std::endl;
    return *ptr_;
}

template<typename T>
T* shared_ptr<T>::operator->() const noexcept
{ return ptr_; }

template<typename T>
T* shared_ptr<T>::get() const noexcept
{ return ptr_; }

template<typename T>
size_t shared_ptr<T>::use_count() const noexcept
{
    if (!ref_count_)
        return 0;
    return *ref_count_;
}

template<typename T>
shared_ptr<T>::operator bool() const noexcept
{ return ptr_ != nullptr; }
#endif // SHARED_PTR_H

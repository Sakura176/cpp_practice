#ifndef UNIQUE_PTR_H
#define UNIQUE_PTR_H

// NOTE: <iostream> 未在头文件中使用，不建议包含（会引入全局流对象）
// 需要 std::swap 时应显式包含 <utility>
#include <utility>

/**
 * @brief 极简 unique_ptr<T>
 *
 * 要求:
 *  1. 独占所有权语义（禁止拷贝）
 *  2. 支持移动语义（转移所有权）
 *  3. 析构时调用 delete
 *  4. operator* 和 operator-> 提供指针访问
 *  5. release() 释放所有权并返回裸指针
 *  6. reset() 销毁当前资源并可选接管新资源
 *  7. 可隐式转换为 bool（检查是否非空）
 *
 * ---- 知识点 ----
 *
 *  独占所有权:
 *    unique_ptr 是 C++ 中表达独占所有权的工具。
 *    拷贝被删除，移动转移所有权。源指针在移动后置空。
 *
 *  移动语义:
 *    移动构造/移动赋值需要将 rhs 的内部指针置空，
 *    确保 rhs 析构时不会 delete 已转移的资源。
 *    注意: rhs 在移动后必须处于有效但未指定的状态。
 *
 *  为什么没有拷贝?
 *    两个 unique_ptr 指向同一资源 → 双重 delete。
 *    如果确实需要共享所有权, 用 shared_ptr。
 *
 *  自定义删除器（选做）:
 *    std::unique_ptr<T, Deleter> 支持自定义删除器。
 *    这个练习不要求实现。
 *
 *  RAII 体现:
 *    new T(...)        → 资源获取
 *    ~unique_ptr()     → 资源释放
 *    move → 所有权转移, 不新增资源
 */

template<typename T>
class unique_ptr
{
public:
    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    /** 默认构造: 空指针 */
    unique_ptr() noexcept;

    /** 从裸指针构造: 接管所有权 */
    explicit unique_ptr(T* ptr) noexcept;

    /** 析构: delete 内部指针 */
    ~unique_ptr();

    // 移动语义
    unique_ptr(unique_ptr&& other) noexcept;
    unique_ptr& operator=(unique_ptr&& other) noexcept;

    // 禁止拷贝
    unique_ptr(const unique_ptr&)            = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    /** 解引用: 获取对象引用 */
    T& operator*() const;
    T* operator->() const;

    /** 获取裸指针（不释放所有权） */
    T* get() const noexcept;

    /**
     * 释放所有权，返回裸指针
     * 调用后内部指针置空，调用方负责 delete
     */
    T* release() noexcept;

    /**
     * 重置: 销毁当前对象，可选接管新指针
     * reset()     → delete 当前对象，置空
     * reset(p)    → delete 当前对象，接管 p
     */
    void reset(T* ptr = nullptr) noexcept;

    /** bool 转换: 检查是否非空 */
    explicit operator bool() const noexcept;

private:
    // 类内初始值（C++11）可避免未初始化风险: T* ptr_ = nullptr;
    T* ptr_;
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================
// NOTE: 使用初始化列表 unique_ptr() noexcept : ptr_(nullptr) {} 更符合 C++ 习惯
template<typename T>
unique_ptr<T>::unique_ptr() noexcept : ptr_(nullptr)
{ /* ptr_ = nullptr; */
}

// NOTE: ptr = nullptr; 置空的是参数（局部变量），对调用方毫无影响
// 应改为初始化列表: unique_ptr(T* ptr) noexcept : ptr_(ptr) {}
// 或直接: this->ptr_ = ptr;（无需置空参数）
template<typename T>
unique_ptr<T>::unique_ptr(T* ptr) noexcept : ptr_(ptr)
{
    // ptr_ = ptr;
    // ptr  = nullptr;
}

template<typename T>
unique_ptr<T>::~unique_ptr()
{
    if (ptr_) {
        delete ptr_;
    }
}

// 【BUG】ptr_ 在构造函数体中未初始化！std::swap 读取未初始化内存 => 未定义行为
// 正确写法:
//   unique_ptr(unique_ptr&& other) noexcept : ptr_(other.release()) {}
// 或:
//   unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
template<typename T>
unique_ptr<T>::unique_ptr(unique_ptr&& other) noexcept : ptr_(other.release())
{
    // ptr_ 未初始化 — std::swap 读取垃圾值到 other.ptr_，后续 delete 可能崩溃
    // std::swap(ptr_, other.ptr_);
    // if (!other) {
    //     delete other.ptr_;
    // }
    // other.ptr_ = nullptr;
}
// NOTE: 功能正确，但通过 swap 绕路实现的模式令人困惑
// 推荐更直接的实现:
//   reset(other.release());
//   return *this;
// 或:
//   if (this != &other) { delete ptr_; ptr_ = other.ptr_; other.ptr_ = nullptr; }
//   return *this;
template<typename T>
unique_ptr<T>& unique_ptr<T>::operator=(unique_ptr&& other) noexcept
{
    // std::swap(ptr_, other.ptr_);
    // if (other) {
    //     delete other.ptr_;
    // }
    // other.ptr_ = nullptr;
    reset(other.release());
    return *this;
}

template<typename T>
T& unique_ptr<T>::operator*() const
{ return *ptr_; }

template<typename T>
T* unique_ptr<T>::operator->() const
{ return ptr_; }

template<typename T>
T* unique_ptr<T>::get() const noexcept
{ return ptr_; }

template<typename T>
T* unique_ptr<T>::release() noexcept
{
    T* ret = ptr_;
    ptr_   = nullptr;
    return ret;
}
// NOTE: reset() 实现正确，简洁明了。
// 注意 delete nullptr 是安全的（C++ 标准保证空 delete 无操作）
// 所以无需 if (ptr_) 判断：delete ptr_; 直接写即可
template<typename T>
void unique_ptr<T>::reset(T* ptr) noexcept
{
    delete ptr_;
    ptr_ = ptr;
}

template<typename T>
unique_ptr<T>::operator bool() const noexcept
{ return ptr_ != nullptr; }
#endif // UNIQUE_PTR_H

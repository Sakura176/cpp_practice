#ifndef UNIQUE_PTR_H
#define UNIQUE_PTR_H

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
    unique_ptr(const unique_ptr&) = delete;
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
    T* ptr_;
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================

#endif  // UNIQUE_PTR_H

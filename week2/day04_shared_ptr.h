#ifndef SHARED_PTR_H
#define SHARED_PTR_H

#include <cstddef>

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
    T*    ptr_;
    int*  ref_count_;  // 堆上分配的引用计数
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================

#endif  // SHARED_PTR_H

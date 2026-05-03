#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "day03_unique_ptr.h"
// NOTE: <iostream> 未在头文件中使用，应移除
#include <cstddef>
#include <iterator>
#include <utility>

/**
 * @brief 使用 unique_ptr 实现的单向链表
 *
 * 要求:
 *  1. 使用 unique_ptr 管理节点，代码中不得出现 delete 关键字
 *  2. push_front / pop_front 基本操作
 *  3. 支持范围 for 循环（提供 begin() / end() 迭代器）
 *  4. 支持移动语义
 *  5. 禁止拷贝
 *
 * ---- 知识点 ----
 *
 *  递归 vs 迭代析构:
 *    unique_ptr 的链式析构是递归的: Node1 析构 → 释放 Node2 → 释放 Node3...
 *    对于很长的链表会导致栈溢出。标准库 std::list 使用迭代析构。
 *    本练习允许递归析构（代码简洁），但面试中应指出这个问题。
 *
 *  为什么链表是测试 unique_ptr 的最佳场景？
 *    unique_ptr 是"零开销抽象"的典型代表 —— 运行时与裸指针无异。
 *    用 unique_ptr 重写链表后，所有资源管理由智能指针自动完成，
 *    不存在手动 delete 遗漏的可能。
 *
 *  为什么不实现 insert/erase？
 *    单向链表的 insert/erase 需要遍历找到前驱节点，
 *    逻辑上有额外复杂度。本练习聚焦"unique_ptr 管理节点生命周期"，
 *    不要求完整的 STL 风格接口。
 */

template<typename T>
class List
{
private:
    struct Node
    {
        T data;
        // TODO: 使用你自己实现的 unique_ptr<T> 或 std::unique_ptr
        // 如果使用自己的 unique_ptr，注意 include "day03_unique_ptr.h"
        unique_ptr<Node> next;

        Node() = default;
        // 【设计】Node(T) 使用转发引用完美转发 data。但 Node() = default 要求 T 可默认构造
        // 这是 dummy head node 设计的硬性约束 — head 始终是一个持有 T 对象的 Node
        // std::forward_list 通过使用 NodeBase（不含 T）作为哨兵来避免此限制
        template<typename U>
        explicit Node(U&& val) : data(std::forward<U>(val)),
                                 next(nullptr)
        {
        }
    };

public:
    // ============================================================
    // TODO: 添加迭代器支持
    // ============================================================

    /**
     * 你需要实现一个简单的正向迭代器，使得下面的代码可以工作:
     *   List<int> lst;
     *   for (int v : lst) { ... }
     *
     * 迭代器最少需要:
     *   operator*  → T&
     *   operator++ → 移动到下一个节点
     *   operator!= → 比较两个迭代器
     */

    // TODO: class iterator { ... };
    // TODO: iterator begin();
    // TODO: iterator end();
    class iterator
    {
    public:
        // NOTE: 缺少标准迭代器类型别名 (iterator_category, value_type, difference_type 等)
        // 导致 std::iterator_traits 无法正确推断，影响与标准算法的兼容性
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T*;
        using reference         = T&;

        iterator(Node* ptr) : ptr_(ptr) {}

        // NOTE: 参数应为 const iterator&，避免不必要的拷贝
        bool operator!=(iterator oth) { return this->ptr_ != oth.ptr_; }

        iterator& operator++()
        {
            ptr_ = this->ptr_->next.get();
            return *this;
        }

        T& operator*() { return ptr_->data; }

    private:
        Node* ptr_;
    };

    iterator begin() { return iterator(head.get()); }
    iterator end() { return iterator(nullptr); }
    class const_iterator
    {
    public:
        const_iterator(const Node* ptr) : ptr_(ptr) {}

        // NOTE: 参数应为 const const_iterator&
        bool operator!=(const_iterator oth) { return this->ptr_ != oth.ptr_; }

        // 【BUG】前缀 ++ 应返回 const_iterator&，此处按值返回
        // 虽因函数体内修改了 ptr_ 而碰巧正确（range-for 中返回值被丢弃），
        // 但不符合 C++ 迭代器约定，链式调用 ++it 会失效
        const_iterator operator++()
        {
            ptr_ = this->ptr_->next.get();
            return *this;
        }

        // NOTE: const_iterator::operator* 应为 const 成员函数: const T& operator*() const
        const T& operator*() { return ptr_->data; }

    private:
        const Node* ptr_;
    };

    const_iterator begin() const { return const_iterator(head.get()); }
    const_iterator end() const { return const_iterator(nullptr); }
    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    List() noexcept;
    ~List();

    // 移动语义
    List(List&& other) noexcept;
    List& operator=(List&& other) noexcept;

    // 禁止拷贝
    List(const List&)            = delete;
    List& operator=(const List&) = delete;

    /** 头部插入 */
    void push_front(const T& value);
    void push_front(T&& value);

    /** 头部删除 */
    void pop_front();

    /** 访问头部元素 */
    T&       front();
    const T& front() const;

    /** 链表是否为空 */
    bool empty() const noexcept;

    /** 返回元素个数 */
    size_t size() const noexcept;

private:
    // TODO: 成员变量
    // 提示: 用头指针管理整个链
    unique_ptr<Node> head;
    size_t           length{0};
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================
// 【设计】默认构造分配一个 dummy head node，要求 T 可默认构造（Node() = default 调用 T())
// 备选设计: head = nullptr 表示空链表，begin()/end() 都返回 nullptr
// 代价是 push_front 需要特殊处理空列表场景
template<typename T>
List<T>::List() noexcept
{ head = unique_ptr<Node>(new Node()); }

// 析构函数为空，unique_ptr<Node> head 自动递归析构全部节点。
// 【注意】对于超长链表（>10万节点），递归析构可能导致栈溢出。
// 生产代码应使用迭代析构: while (head) head = std::move(head->next);
template<typename T>
List<T>::~List()
{
}

template<typename T>
List<T>::List(List&& other) noexcept : head(std::move(other.head)),
                                       length(other.length)
{
    other.head   = unique_ptr<Node>(new Node());
    other.length = 0;
}

template<typename T>
List<T>& List<T>::operator=(List&& other) noexcept
{
    if (this == &other)
        return *this;

    head       = std::move(other.head);
    other.head = unique_ptr<Node>(new Node());
    length     = std::exchange(other.length, 0);
    return *this;
}

template<typename T>
void List<T>::push_front(const T& val)
{
    auto node  = unique_ptr<Node>(new Node(val));
    node->next = std::move(head->next);
    head->next = std::move(node);
    length++;
}

template<typename T>
void List<T>::push_front(T&& val)
{
    auto node  = unique_ptr<Node>(new Node(std::move(val)));
    node->next = std::move(head->next);
    head->next = std::move(node);
    length++;
}

// 【实现说明】将头部节点移出链表，unique_ptr 自动释放。
// 变量 node 持有被弹出节点的所有权，函数结束时自动 delete。
//
// NOTE: length 是 size_t（无符号类型），length <= 0 等价于 length == 0
// 建议改为 if (length == 0) 避免 -Wtype-limits 警告
template<typename T>
void List<T>::pop_front()
{
    if (length <= 0)
        return;
    auto node  = std::move(head->next);
    head->next = std::move(node->next);
    length--;
}

// 【BUG】空链表时 head->next == nullptr，解引用 nullptr → UB
// std::forward_list::front() 同样规定"空列表行为未定义"，
// 但当前代码没有任何防御或文档说明此前提条件
template<typename T>
T& List<T>::front()
{ return head->next->data; }

template<typename T>
const T& List<T>::front() const
{ return head->next->data; }

template<typename T>
bool List<T>::empty() const noexcept
{ return length == 0; }

template<typename T>
size_t List<T>::size() const noexcept
{ return length; }

#endif // LINKED_LIST_H

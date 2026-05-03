#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "day03_unique_ptr.h"
#include <cstddef>
#include <iostream>
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
        iterator(Node* ptr) : ptr_(ptr) {}

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

        bool operator!=(const_iterator oth) { return this->ptr_ != oth.ptr_; }

        const_iterator operator++()
        {
            ptr_ = this->ptr_->next.get();
            return *this;
        }

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
template<typename T>
List<T>::List() noexcept
{ head = unique_ptr<Node>(new Node()); }

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

template<typename T>
void List<T>::pop_front()
{
    if (length <= 0)
        return;
    auto node  = std::move(head->next);
    head->next = std::move(node->next);
    length--;
}

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

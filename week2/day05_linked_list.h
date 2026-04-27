#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <cstddef>
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
        Node* next;

        Node(const T& val) : data(val), next(nullptr) {}
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

    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    List() noexcept;
    ~List();

    // 移动语义
    List(List&& other) noexcept;
    List& operator=(List&& other) noexcept;

    // 禁止拷贝
    List(const List&) = delete;
    List& operator=(const List&) = delete;

    /** 头部插入 */
    void push_front(const T& value);

    /** 头部删除 */
    void pop_front();

    /** 访问头部元素 */
    T& front();
    const T& front() const;

    /** 链表是否为空 */
    bool empty() const noexcept;

    /** 返回元素个数 */
    size_t size() const noexcept;

private:
    // TODO: 成员变量
    // 提示: 用头指针管理整个链
};

// ============================================================
// TODO: 在这里实现模板成员函数
// ============================================================

#endif  // LINKED_LIST_H

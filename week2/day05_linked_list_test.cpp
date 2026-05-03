/**
 * @brief 链表 (unique_ptr 版) 测试套件（GTest）
 *
 * 测试覆盖:
 *  1. 默认构造为空链表
 *  2. push_front 和 front()
 *  3. pop_front
 *  4. 多种类型支持
 *  5. 移动语义
 *  6. 析构释放所有节点（Tracker 模式验证）
 *  7. 边界条件
 *  8. 大量节点
 *  9. 范围 for 循环（如果实现了迭代器）
 */

#include "day05_linked_list.h"
#include <gtest/gtest.h>

#include <string>

// -----------------------------------------------------------
// 辅助: 跟踪构造/析构的节点
// -----------------------------------------------------------

struct Tracker
{
    static int alive;
    int        id;

    Tracker(int id = 0) : id(id) { alive++; }
    ~Tracker() { alive--; }
    Tracker(const Tracker&)            = delete;
    Tracker& operator=(const Tracker&) = delete;
    Tracker(Tracker&& oth) noexcept
    {
        id     = oth.id;
        oth.id = 0;
        alive++;
    }
};
int Tracker::alive = 0;

// -----------------------------------------------------------
// 测试 1: 默认构造
// -----------------------------------------------------------

TEST(Day05, default_construct)
{
    List<int> lst;
    EXPECT_TRUE(lst.empty()) << "new list is empty";
    EXPECT_EQ(lst.size(), 0u) << "size is 0";
}

// -----------------------------------------------------------
// 测试 2: push_front 和 front()
// -----------------------------------------------------------

TEST(Day05, push_front)
{
    List<int> lst;
    lst.push_front(10);
    EXPECT_FALSE(lst.empty()) << "not empty after push";
    EXPECT_EQ(lst.size(), 1u) << "size is 1";
    EXPECT_EQ(lst.front(), 10) << "front is 10";

    lst.push_front(20);
    EXPECT_EQ(lst.size(), 2u) << "size is 2";
    EXPECT_EQ(lst.front(), 20) << "front is 20 (new head)";
}

// -----------------------------------------------------------
// 测试 3: pop_front
// -----------------------------------------------------------

TEST(Day05, pop_front)
{
    List<int> lst;
    lst.push_front(1);
    lst.push_front(2);
    lst.push_front(3);
    EXPECT_EQ(lst.size(), 3u) << "size 3 before pop";

    lst.pop_front();
    EXPECT_EQ(lst.size(), 2u) << "size 2 after one pop";
    EXPECT_EQ(lst.front(), 2) << "front is 2";

    lst.pop_front();
    EXPECT_EQ(lst.size(), 1u) << "size 1";
    EXPECT_EQ(lst.front(), 1) << "front is 1";

    lst.pop_front();
    EXPECT_TRUE(lst.empty()) << "empty after all pops";
    EXPECT_EQ(lst.size(), 0u) << "size 0";
}

// -----------------------------------------------------------
// 测试 4: 多种类型
// -----------------------------------------------------------

TEST(Day05, string_list)
{
    List<std::string> lst;
    lst.push_front("world");
    lst.push_front("hello");
    EXPECT_EQ(lst.front(), "hello") << "string front";
    lst.pop_front();
    EXPECT_EQ(lst.front(), "world") << "string after pop";
}

// -----------------------------------------------------------
// 测试 5: 移动语义
// -----------------------------------------------------------

TEST(Day05, move_constructor)
{
    List<int> lst1;
    lst1.push_front(1);
    lst1.push_front(2);

    List<int> lst2(std::move(lst1));
    EXPECT_TRUE(lst1.empty()) << "source empty after move";
    EXPECT_EQ(lst2.size(), 2u) << "target has elements";
    EXPECT_EQ(lst2.front(), 2) << "target front is 2";
}

TEST(Day05, move_assignment)
{
    List<int> lst1;
    lst1.push_front(1);
    lst1.push_front(2);

    List<int> lst2;
    lst2.push_front(99);
    lst2 = std::move(lst1);

    EXPECT_TRUE(lst1.empty()) << "source empty after move assign";
    EXPECT_EQ(lst2.size(), 2u) << "target has elements";
    EXPECT_EQ(lst2.front(), 2) << "target front is 2";
}

// -----------------------------------------------------------
// 测试 6: 析构释放所有节点
// -----------------------------------------------------------

TEST(Day05, destruction_frees_nodes)
{
    Tracker::alive = 0;
    {
        List<Tracker> lst;
        lst.push_front(Tracker(1));
        lst.push_front(Tracker(2));
        lst.push_front(Tracker(3));
        EXPECT_TRUE(Tracker::alive > 0) << "trackers alive during list lifetime";
    }
    EXPECT_EQ(Tracker::alive, 0) << "all trackers destroyed with list";
}

// -----------------------------------------------------------
// 测试 7: 边界条件 — 对空链表 pop_front
// -----------------------------------------------------------

TEST(Day05, pop_front_empty)
{
    List<int> lst;
    lst.pop_front();
    lst.pop_front();
    EXPECT_TRUE(lst.empty()) << "still empty after popping empty list";
}

// -----------------------------------------------------------
// 测试 8: 大量节点
// -----------------------------------------------------------

TEST(Day05, many_elements)
{
    List<int>     lst;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) {
        lst.push_front(i);
    }
    EXPECT_EQ(lst.size(), static_cast<size_t>(N)) << "all elements pushed";

    int expected = N - 1;
    while (!lst.empty()) {
        EXPECT_EQ(lst.front(), expected) << "value matches";
        lst.pop_front();
        expected--;
    }
    EXPECT_TRUE(lst.empty()) << "all popped";
}

// -----------------------------------------------------------
// 测试 9: 范围 for 循环（需实现迭代器）
// -----------------------------------------------------------

TEST(Day05, range_for)
{
    List<int> lst;
    lst.push_front(3);
    lst.push_front(2);
    lst.push_front(1);

    int sum = 0;
    for (int v : lst) {
        sum += v;
    }
    EXPECT_EQ(sum, 6) << "range-for sum = 1+2+3";

    const List<int>& clst = lst;
    sum                   = 0;
    for (int v : clst) {
        sum += v;
    }
    EXPECT_EQ(sum, 6) << "const range-for works";
}

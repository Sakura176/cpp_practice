/**
 * @brief shared_ptr 测试套件（GTest）
 *
 * 测试覆盖:
 *  1. 默认构造为 nullptr
 *  2. 从裸指针构造，引用计数初始为 1
 *  3. 拷贝构造增加引用计数
 *  4. 析构减少引用计数
 *  5. 拷贝赋值正确处理新旧对象
 *  6. 最后一个引用析构时 delete 资源
 *  7. 移动语义（不改变引用计数）
 *  8. 解引用
 *  9. use_count() 精确性
 *  10. bool 转换
 *  11. 多个 shared_ptr 共享所有权
 */

#include <gtest/gtest.h>
#include "day04_shared_ptr.h"

#include <string>
#include <utility>

// ============================================================
// 辅助: 跟踪构造/析构的对象
// ============================================================

struct Tracker
{
    static int alive;
    static int total_constructed;

    int id;
    Tracker(int id = 0) : id(id) {
        alive++;
        total_constructed++;
    }
    ~Tracker() { alive--; }
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    int get_id() const { return id; }
};
int Tracker::alive = 0;
int Tracker::total_constructed = 0;

// -----------------------------------------------------------
// 测试 1: 默认构造
// -----------------------------------------------------------

TEST(Day04, default_construct)
{
    shared_ptr<int> sp;
    EXPECT_FALSE(sp) << "default shared_ptr is null";
    EXPECT_EQ(sp.get(), nullptr) << "get() returns nullptr";
    EXPECT_EQ(sp.use_count(), 0u) << "null shared_ptr has count 0";
}

// -----------------------------------------------------------
// 测试 2: 从裸指针构造
// -----------------------------------------------------------

TEST(Day04, ptr_construct)
{
    Tracker::alive = 0;

    shared_ptr<Tracker> sp(new Tracker(10));
    EXPECT_TRUE(sp) << "shared_ptr owns something";
    EXPECT_EQ(sp.use_count(), 1u) << "initial refcount is 1";
    EXPECT_EQ(Tracker::alive, 1) << "object alive";
}

// -----------------------------------------------------------
// 测试 3: 拷贝构造增加引用计数
// -----------------------------------------------------------

TEST(Day04, copy_construct_increases_count)
{
    shared_ptr<Tracker> sp1(new Tracker(1));
    EXPECT_EQ(sp1.use_count(), 1u) << "count 1 after construct";

    {
        shared_ptr<Tracker> sp2(sp1);
        EXPECT_EQ(sp1.use_count(), 2u) << "count 2 after copy (via sp1)";
        EXPECT_EQ(sp2.use_count(), 2u) << "count 2 after copy (via sp2)";
        EXPECT_EQ(Tracker::alive, 1) << "one object, two shared_ptrs";

        shared_ptr<Tracker> sp3(sp2);
        EXPECT_EQ(sp1.use_count(), 3u) << "count 3 after second copy";
    }

    EXPECT_EQ(sp1.use_count(), 1u) << "count back to 1 after sp2,sp3 destroyed";
    EXPECT_EQ(Tracker::alive, 1) << "object still alive (sp1 holds it)";
}

// -----------------------------------------------------------
// 测试 4: 最后一个引用析构时销毁资源
// -----------------------------------------------------------

TEST(Day04, last_reference_destroys)
{
    Tracker::alive = 0;
    {
        shared_ptr<Tracker> sp1(new Tracker(1));
        shared_ptr<Tracker> sp2(sp1);
        EXPECT_EQ(Tracker::alive, 1) << "object alive";
    }
    EXPECT_EQ(Tracker::alive, 0) << "object destroyed when last shared_ptr dies";
}

// -----------------------------------------------------------
// 测试 5: 拷贝赋值
// -----------------------------------------------------------

TEST(Day04, copy_assignment)
{
    Tracker::alive = 0;

    shared_ptr<Tracker> sp1(new Tracker(1));
    shared_ptr<Tracker> sp2(new Tracker(2));
    EXPECT_EQ(Tracker::alive, 2) << "two objects alive";

    sp2 = sp1;
    EXPECT_EQ(Tracker::alive, 1) << "one object remains after assign";
    EXPECT_EQ(sp1.use_count(), 2u) << "count 2 after assign (sp1)";
    EXPECT_EQ(sp2.use_count(), 2u) << "count 2 after assign (sp2)";
    EXPECT_EQ(sp2->get_id(), 1) << "sp2 now points to sp1's object";
}

// -----------------------------------------------------------
// 测试 6: 移动语义
// -----------------------------------------------------------

TEST(Day04, move_construct)
{
    shared_ptr<Tracker> sp1(new Tracker(1));
    EXPECT_EQ(sp1.use_count(), 1u) << "count 1 before move";

    shared_ptr<Tracker> sp2(std::move(sp1));
    EXPECT_FALSE(sp1) << "sp1 null after move";
    EXPECT_TRUE(sp2) << "sp2 owns resource";
    EXPECT_EQ(sp2.use_count(), 1u) << "count still 1 (ownership transferred, not shared)";
    EXPECT_EQ(Tracker::alive, 1) << "object still alive";
}

TEST(Day04, move_assign)
{
    shared_ptr<Tracker> sp1(new Tracker(1));
    shared_ptr<Tracker> sp2(new Tracker(2));

    sp2 = std::move(sp1);
    EXPECT_FALSE(sp1) << "sp1 null after move assign";
    EXPECT_TRUE(sp2) << "sp2 owns resource";
    EXPECT_EQ(sp2.use_count(), 1u) << "count 1 after move assign";
    EXPECT_EQ(Tracker::alive, 1) << "only sp2's old object destroyed";
}

// -----------------------------------------------------------
// 测试 7: 解引用
// -----------------------------------------------------------

TEST(Day04, dereference)
{
    shared_ptr<int> sp(new int(42));
    EXPECT_EQ(*sp, 42) << "operator* works";

    shared_ptr<std::string> sps(new std::string("shared"));
    EXPECT_EQ(sps->size(), 6u) << "operator-> works";
}

// -----------------------------------------------------------
// 测试 8: use_count 精确性
// -----------------------------------------------------------

TEST(Day04, use_count_precision)
{
    shared_ptr<int> sp1(new int(0));
    EXPECT_EQ(sp1.use_count(), 1u) << "count 1";

    shared_ptr<int> sp2(sp1);
    EXPECT_EQ(sp1.use_count(), 2u) << "count 2 after copy";

    shared_ptr<int> sp3(sp2);
    EXPECT_EQ(sp1.use_count(), 3u) << "count 3";

    // sp3.reset();  // 可选: 如果实现了 reset() 可取消注释
    {
        shared_ptr<int> sp4(sp1);
        EXPECT_EQ(sp1.use_count(), 4u) << "count 4";
    }
    EXPECT_EQ(sp1.use_count(), 3u) << "back to 3";
}

// -----------------------------------------------------------
// 测试 9: bool 转换
// -----------------------------------------------------------

TEST(Day04, bool_conversion)
{
    shared_ptr<int> empty;
    shared_ptr<int> valid(new int(0));

    EXPECT_FALSE(empty) << "empty is false";
    EXPECT_TRUE(valid) << "valid is true";
}

// -----------------------------------------------------------
// 测试 10: 共享所有权
// -----------------------------------------------------------

TEST(Day04, shared_ownership)
{
    Tracker::alive = 0;

    shared_ptr<Tracker> sp1(new Tracker(1));
    shared_ptr<Tracker> sp2(sp1);
    shared_ptr<Tracker> sp3(sp2);

    // sp1.reset();  // 可选: 如果实现了 reset() 可取消注释
    {
        shared_ptr<Tracker> sp4(sp2);
        EXPECT_EQ(Tracker::alive, 1) << "still alive with 4 refs";
    }

    EXPECT_EQ(Tracker::alive, 1) << "still alive with 3 refs";
}

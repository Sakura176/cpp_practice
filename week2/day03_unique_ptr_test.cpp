/**
 * @brief unique_ptr 测试套件（GTest）
 *
 * 测试覆盖:
 *  1. 默认构造为 nullptr
 *  2. 从裸指针构造和析构自动 delete
 *  3. 移动语义（转移所有权）
 *  4. 解引用 operator* 和 operator->
 *  5. release() 释放所有权
 *  6. reset() 重置指针
 *  7. bool 转换
 *  8. 在容器中移动（vector）
 *  9. 禁止拷贝（编译期检查）
 */

#include <gtest/gtest.h>
#include "day03_unique_ptr.h"

#include <string>
#include <utility>
#include <vector>

// ============================================================
// 辅助: 跟踪构造/析构的对象
// ============================================================

struct Tracker
{
    static int alive;

    int id;
    Tracker(int id = 0) : id(id) { alive++; }
    ~Tracker() { alive--; }
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    int get_id() const { return id; }
};
int Tracker::alive = 0;

// -----------------------------------------------------------
// 测试 1: 默认构造为 nullptr
// -----------------------------------------------------------

TEST(Day03, default_construct)
{
    unique_ptr<int> p;
    EXPECT_FALSE(p) << "default constructed ptr is null";
    EXPECT_EQ(p.get(), nullptr) << "get() returns nullptr";
}

// -----------------------------------------------------------
// 测试 2: 从裸指针构造，析构自动 delete
// -----------------------------------------------------------

TEST(Day03, ownership_and_destruction)
{
    EXPECT_EQ(Tracker::alive, 0) << "no trackers yet";

    {
        unique_ptr<Tracker> p(new Tracker(42));
        EXPECT_EQ(Tracker::alive, 1) << "tracker alive after construction";
        EXPECT_NE(p.get(), nullptr) << "pointer is valid";
    }

    EXPECT_EQ(Tracker::alive, 0) << "tracker destroyed by unique_ptr destructor";
}

// -----------------------------------------------------------
// 测试 3: 移动语义
// -----------------------------------------------------------

TEST(Day03, move_constructor)
{
    unique_ptr<Tracker> p1(new Tracker(1));
    EXPECT_EQ(Tracker::alive, 1) << "alive after p1 construction";

    unique_ptr<Tracker> p2(std::move(p1));
    EXPECT_EQ(Tracker::alive, 1) << "still alive after move";
    EXPECT_FALSE(p1) << "p1 is null after move";
    EXPECT_TRUE(p2) << "p2 is not null";
    EXPECT_EQ(p1.get(), nullptr) << "p1 internal pointer null";
    EXPECT_NE(p2.get(), nullptr) << "p2 has the pointer";
}

TEST(Day03, move_assignment)
{
    unique_ptr<Tracker> p1(new Tracker(1));
    unique_ptr<Tracker> p2(new Tracker(2));

    EXPECT_EQ(Tracker::alive, 2) << "both alive before move";

    p2 = std::move(p1);
    EXPECT_EQ(Tracker::alive, 1) << "one remains after move-assign";
    EXPECT_FALSE(p1) << "p1 null after move";
    EXPECT_TRUE(p2) << "p2 owns the resource";
}

// -----------------------------------------------------------
// 测试 4: 解引用
// -----------------------------------------------------------

TEST(Day03, dereference)
{
    unique_ptr<int> p(new int(42));
    EXPECT_EQ(*p, 42) << "operator* works";

    unique_ptr<std::string> sp(new std::string("hello"));
    EXPECT_EQ(sp->size(), 5u) << "operator-> works";
}

// -----------------------------------------------------------
// 测试 5: release()
// -----------------------------------------------------------

TEST(Day03, release)
{
    Tracker::alive = 0;
    Tracker* raw;
    {
        unique_ptr<Tracker> p(new Tracker(99));
        raw = p.release();
        EXPECT_FALSE(p) << "p null after release";
        EXPECT_NE(raw, nullptr) << "raw pointer is valid";
        EXPECT_EQ(Tracker::alive, 1) << "object still alive";
    }
    EXPECT_EQ(Tracker::alive, 1) << "unique_ptr destructor did not delete";

    delete raw;
    EXPECT_EQ(Tracker::alive, 0) << "manually deleted";
}

// -----------------------------------------------------------
// 测试 6: reset()
// -----------------------------------------------------------

TEST(Day03, reset)
{
    Tracker::alive = 0;

    unique_ptr<Tracker> p(new Tracker(1));
    EXPECT_EQ(Tracker::alive, 1) << "one alive";

    p.reset(new Tracker(2));
    EXPECT_EQ(Tracker::alive, 1) << "old deleted, new alive";

    p.reset();
    EXPECT_FALSE(p) << "p null after reset()";
    EXPECT_EQ(Tracker::alive, 0) << "all deleted";
}

// -----------------------------------------------------------
// 测试 7: bool 转换
// -----------------------------------------------------------

TEST(Day03, bool_conversion)
{
    unique_ptr<int> empty;
    unique_ptr<int> valid(new int(0));

    EXPECT_FALSE(empty) << "empty should be false";
    EXPECT_TRUE(valid) << "valid should be true";

    EXPECT_FALSE(static_cast<bool>(empty)) << "explicit bool: empty";
    EXPECT_TRUE(static_cast<bool>(valid)) << "explicit bool: valid";
}

// -----------------------------------------------------------
// 测试 8: 放入容器
// -----------------------------------------------------------

TEST(Day03, move_into_container)
{
    std::vector<unique_ptr<int>> vec;
    vec.push_back(unique_ptr<int>(new int(10)));
    vec.push_back(unique_ptr<int>(new int(20)));

    EXPECT_EQ(vec.size(), 2u) << "two elements in vector";
    EXPECT_EQ(*vec[0], 10) << "first element";
    EXPECT_EQ(*vec[1], 20) << "second element";
}

// -----------------------------------------------------------
// 测试 9: 禁止拷贝（编译期验证）
// -----------------------------------------------------------

TEST(Day03, no_copy)
{
    unique_ptr<int> p(new int(1));
    // unique_ptr<int> q = p;    // 拷贝构造 → delete
    // unique_ptr<int> r; r = p; // 拷贝赋值 → delete
    (void)p;
}

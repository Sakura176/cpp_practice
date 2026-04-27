/**
 * @brief FileReader 测试套件（GTest）
 *
 * 测试覆盖:
 *  1. 正常打开和读取文件
 *  2. 文件不存在时构造
 *  3. 析构自动关闭文件
 *  4. 移动语义
 *  5. 禁止拷贝（编译期检查）
 *  6. 反复读取直到 EOF
 *  7. 空文件
 */

#include "day01_file_reader.h"
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

// -----------------------------------------------------------
// 辅助: 创建临时测试文件
// -----------------------------------------------------------

static std::string create_temp_file(const std::string& content)
{
    std::string   path = std::tmpnam(nullptr);
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path;
}

static void remove_temp_file(const std::string& path)
{ std::remove(path.c_str()); }

// -----------------------------------------------------------
// 测试 1: 正常读取
// -----------------------------------------------------------

TEST(Day01, normal_read)
{
    auto path = create_temp_file("hello\nworld\n");
    {
        FileReader reader(path);
        EXPECT_TRUE(reader.is_open()) << "file should be open after construction";

        auto line = reader.read_line();
        EXPECT_TRUE(line.has_value()) << "read first line";
        EXPECT_EQ(line, "hello") << "first line content";

        line = reader.read_line();
        EXPECT_TRUE(line.has_value()) << "read second line";
        EXPECT_EQ(line, "world") << "second line content";

        EXPECT_FALSE(reader.read_line().has_value()) << "EOF reached";
    }
    remove_temp_file(path);
}

// -----------------------------------------------------------
// 测试 2: 文件不存在
// -----------------------------------------------------------

TEST(Day01, file_not_found)
{ EXPECT_THROW(FileReader reader("/tmp/nonexistent_file_xyz"), std::runtime_error); }

// -----------------------------------------------------------
// 测试 3: 析构自动关闭
// -----------------------------------------------------------

TEST(Day01, raii_close)
{
    auto path = create_temp_file("data");
    {
        FileReader reader(path);
        EXPECT_TRUE(reader.is_open()) << "file open within scope";
    }
    bool removed = (std::remove(path.c_str()) == 0);
    EXPECT_TRUE(removed) << "file was closed by destructor, can be removed";
}

// -----------------------------------------------------------
// 测试 4: 移动语义
// -----------------------------------------------------------

TEST(Day01, move_constructor)
{
    auto path = create_temp_file("movable");
    {
        FileReader reader(path);
        EXPECT_TRUE(reader.is_open()) << "source open";

        FileReader reader2(std::move(reader));
        EXPECT_TRUE(reader2.is_open()) << "target open after move";
        EXPECT_FALSE(reader.is_open()) << "source closed after move";

        auto line = reader2.read_line();
        EXPECT_TRUE(line.has_value()) << "read after move";
        EXPECT_EQ(line, "movable") << "content correct";
    }
    remove_temp_file(path);
}

TEST(Day01, move_assignment)
{
    auto path1 = create_temp_file("file1");
    auto path2 = create_temp_file("file2");
    {
        FileReader r1(path1);
        FileReader r2(path2);
        r2 = std::move(r1);
        EXPECT_TRUE(r2.is_open()) << "target open after move assign";
        auto line = r2.read_line();
        EXPECT_TRUE(line.has_value()) << "read from moved-to object";
        EXPECT_EQ(line, "file1") << "content from source";
    }
    remove_temp_file(path1);
    remove_temp_file(path2);
}

// -----------------------------------------------------------
// 测试 5: 禁止拷贝（编译期验证，取消注释应编译失败）
// -----------------------------------------------------------

TEST(Day01, no_copy)
{
    auto       path = create_temp_file("x");
    FileReader r(path);
    // FileReader r2 = r;     // 拷贝构造 → delete（取消注释应编译失败）
    // FileReader r3; r3 = r; // 拷贝赋值 → delete（取消注释应编译失败）
    (void)r;
    remove_temp_file(path);
}

// -----------------------------------------------------------
// 测试 6: 空文件
// -----------------------------------------------------------

TEST(Day01, empty_file)
{
    auto path = create_temp_file("");
    {
        FileReader reader(path);
        EXPECT_TRUE(reader.is_open()) << "empty file opens ok";

        auto line = reader.read_line();
        EXPECT_FALSE(line.has_value()) << "empty file → immediate EOF";
    }
    remove_temp_file(path);
}

// -----------------------------------------------------------
// 测试 7: 大文件逐行读取
// -----------------------------------------------------------

TEST(Day01, many_lines)
{
    std::string content;
    for (int i = 0; i < 100; ++i) {
        content += "line " + std::to_string(i) + "\n";
    }
    auto path = create_temp_file(content);
    {
        FileReader reader(path);
        auto       line  = reader.read_line();
        int        count = 0;
        while (line.has_value()) {
            std::string expected = "line " + std::to_string(count);
            EXPECT_EQ(line, expected) << "line " << count;
            count++;
            line = reader.read_line();
        }
        EXPECT_EQ(count, 100) << "all 100 lines read";
    }
    remove_temp_file(path);
}

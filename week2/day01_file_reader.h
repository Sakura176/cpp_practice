#ifndef FILE_READER_H
#define FILE_READER_H

#include <fstream>
#include <optional>
#include <string>

/**
 * @brief RAII 文件读取器
 *
 * 要求:
 *  1. 构造时打开文件（传入文件名）
 *  2. 析构时自动关闭文件
 *  3. 提供 read_line() 逐行读取
 *  4. 提供 is_open() 检查文件状态
 *  5. 支持移动语义（转移文件句柄所有权）
 *  6. 禁止拷贝语义
 *
 * ---- 知识点 ----
 *  RAII (Resource Acquisition Is Initialization):
 *    资源获取即初始化，资源释放由析构函数保证。
 *    这是 C++ 区别于其他语言的核心设计哲学。
 *
 *  文件句柄的移动语义:
 *    std::ifstream 本身支持移动，FileReader 应利用
 *    这一特性实现移动构造/移动赋值。
 *
 *  如果打开失败:
 *    可以抛出异常，或者设置内部状态由 is_open() 检查。
 *    （这个问题在面试中常被拿来讨论 "constructor failure"）
 */

class FileReader
{
public:
    // ============================================================
    // TODO: 实现以下接口
    // ============================================================

    /**
     * 构造函数: 打开指定文件
     * 实现方案 A: 打开失败时抛 std::runtime_error
     * 实现方案 B: 不抛异常，用 is_open() 检查
     * 无论哪种方案，析构时都要安全关闭
     */
    explicit FileReader(const std::string& filename);

    ~FileReader();

    // 移动语义
    FileReader(FileReader&& other) noexcept;
    FileReader& operator=(FileReader&& other) noexcept;

    // 禁止拷贝
    FileReader(const FileReader&)            = delete;
    FileReader& operator=(const FileReader&) = delete;

    /** 文件是否成功打开 */
    bool is_open() const;

    /**
     * 读取一行
     * 返回 false 表示读到文件末尾或出错
     * 读取的内容通过 line 参数返回
     */
    std::optional<std::string> read_line();

private:
    std::ifstream file_;
};

#endif // FILE_READER_H

#include "day01_file_reader.h"
#include <ios>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

// ============================================================
// FileReader — 实现骨架
//
// 说明: 将下方的 TODO 替换为完整的实现。
// 完成后运行 day01_file_reader_test 验证正确性。
// ============================================================

FileReader::FileReader(const std::string& filename)
{
    file_.open(filename);
    if (!file_.is_open()) {
        // 使用抛异常方案，打印错误信息后续仍可能被调用
        throw std::runtime_error("file can not open: " + filename);
    }
}

FileReader::~FileReader()
{
}

FileReader::FileReader(FileReader&& other) noexcept : file_(std::move(other.file_))
{
}

FileReader& FileReader::operator=(FileReader&& other) noexcept
{
    file_ = std::move(other.file_);
    return *this;
}

bool FileReader::is_open() const
{ return file_.is_open(); }

std::optional<std::string> FileReader::read_line()
{
    // TODO: 考虑优化区分读到末尾的情况
    if (file_.is_open()) {
        std::string line;

        bool ret = static_cast<bool>(std::getline(file_, line));
        if (!ret) {
            return std::nullopt;
        }
        return std::optional<std::string>{line};
    }

    std::cout << "file is not opened" << std::endl;
    return std::nullopt;
}

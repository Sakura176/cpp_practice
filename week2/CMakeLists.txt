# ============================================================
# 第 2 周：RAII 与资源管理
# ============================================================
# Day01-02: 非模板类 → 头文件 + 实现(.cpp) + 测试
# Day03-04: 模板类   → 头文件(含实现) + 测试
# Day05:    综合练习  → 头文件 + 实现 + 测试

# ---- Day01: FileReader（非模板类）----
add_practice_test(day01_file_reader
    "day01_file_reader.cpp"
    "day01_file_reader_test.cpp"
)

# ---- Day02: ScopedMutex（模板类）----
add_practice_header_only_test(day02_scoped_mutex
    "day02_scoped_mutex_test.cpp"
)

# ---- Day03: unique_ptr（模板类）----
add_practice_header_only_test(day03_unique_ptr
    "day03_unique_ptr_test.cpp"
)

# ---- Day04: shared_ptr（模板类）----
add_practice_header_only_test(day04_shared_ptr
    "day04_shared_ptr_test.cpp"
)

# ---- Day05: 链表（模板类）----
add_practice_header_only_test(day05_linked_list
    "day05_linked_list_test.cpp"
)

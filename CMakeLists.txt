cmake_minimum_required(VERSION 3.16)
project(cpp-practice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Google Test（使用系统源码 /usr/src/googletest）
# ============================================================
enable_testing()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
if(NOT TARGET GTest::gtest)
    add_subdirectory(/usr/src/googletest ${CMAKE_BINARY_DIR}/_deps/gtest-build
        EXCLUDE_FROM_ALL)
endif()
include(GoogleTest)

# ============================================================
# 公共编译选项
# ============================================================

# 用于"实现即运行"模式（Week 1 风格：单 .cpp 自包含）
function(add_practice_day FILE_PATH)
    get_filename_component(DAY_NAME ${FILE_PATH} NAME_WE)
    add_executable(${DAY_NAME} ${FILE_PATH})
    target_include_directories(${DAY_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
    )
    target_link_libraries(${DAY_NAME} pthread)
    target_compile_options(${DAY_NAME} PRIVATE -fsanitize=thread -g)
    target_link_options(${DAY_NAME} PRIVATE -fsanitize=thread)
endfunction()

# 用于"头文件接口 + 实现 + 测试"模式（非模板类）
function(add_practice_test DAY_NAME IMPL_PATH TEST_PATH)
    add_executable(${DAY_NAME}_test ${IMPL_PATH} ${TEST_PATH})
    target_include_directories(${DAY_NAME}_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
    )
    target_link_libraries(${DAY_NAME}_test GTest::gtest GTest::gtest_main pthread)
    target_compile_options(${DAY_NAME}_test PRIVATE -g)
    gtest_discover_tests(${DAY_NAME}_test)
endfunction()

# 用于"模板类头文件 + 测试"模式
function(add_practice_header_only_test DAY_NAME TEST_PATH)
    add_executable(${DAY_NAME}_test ${TEST_PATH})
    target_include_directories(${DAY_NAME}_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
    )
    target_link_libraries(${DAY_NAME}_test GTest::gtest GTest::gtest_main)
    target_compile_options(${DAY_NAME}_test PRIVATE -g)
    gtest_discover_tests(${DAY_NAME}_test)
endfunction()

# ============================================================
# 各周子目录
# ============================================================

add_subdirectory(week1)
add_subdirectory(week2)
# add_subdirectory(week3)
# add_subdirectory(week4)
# add_subdirectory(week5)
# add_subdirectory(week6)
# add_subdirectory(week7)
# add_subdirectory(week8)

cmake_minimum_required(VERSION 3.16)
project(cpp-practice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动搜索所有子目录并添加为独立可执行文件
function(add_practice_day FILE_PATH)
    get_filename_component(DAY_NAME ${FILE_PATH} NAME_WE)
    add_executable(${DAY_NAME} ${FILE_PATH})
    target_include_directories(${DAY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# 添加各周子目录，每个 .cpp 自动生成一个可执行文件
add_subdirectory(week1)
# add_subdirectory(week2)
# add_subdirectory(week3)
# add_subdirectory(week4)
# add_subdirectory(week5)
# add_subdirectory(week6)
# add_subdirectory(week7)
# add_subdirectory(week8)

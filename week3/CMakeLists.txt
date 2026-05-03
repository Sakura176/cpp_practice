# ============================================================
# 第 3 周：生产者 - 消费者专题（高频手撕）
# ============================================================
# 使用"实现即运行"模式：单 .cpp 自包含，含 main()
# 编译: cd build && cmake .. && make dayXX_name && ./build/week3/dayXX_name
# 线程检查通过 -fsanitize=thread 启用

file(GLOB DAY_SOURCES "day*.cpp")
foreach(SOURCE ${DAY_SOURCES})
    add_practice_day(${SOURCE})
endforeach()

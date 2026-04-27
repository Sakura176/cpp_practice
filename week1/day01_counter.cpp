#include <iostream>

/**
 * @brief 基础 Counter 类（单线程版本）
 *
 * 知识点:
 *  1. 封装: count 为 private，通过公有接口访问 —— C++ 类的基础封装
 *  2. 默认初始化: count{0} 使用列表初始化，优于 count = 0（列表初始化禁止窄化转换）
 *
 * 追问:
 *  - Q: 这个类线程安全吗？
 *  - A: 不安全。count++ 本质是 read-modify-write 三步骤，多线程下会有数据竞争。
 */
class Counter
{
public:
    void increment() { count++; }
    void decrement() { count--; }
    long get() const { return count; }

private:
    long count{0};
};

// 注: main 的 argc/argv 未使用 —— 可写为 int main() 或 (void)argc
int main()
{
    Counter count;
    count.increment();
    // std::endl 会刷新缓冲区，频繁调用影响性能
    // 多数场景下用 '\n' 即可，仅在需要立即输出时用 endl
    std::cout << "count: " << count.get() << '\n';
    count.decrement();
    std::cout << "count: " << count.get() << '\n';
    return 0;
}

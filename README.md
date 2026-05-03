# C++ 开发工程师面试备战手册

> 本手册用于系统提升 C++ 代码组织能力、温习核心基础、备战技术面试。  
> 每完成一项，请在 `[ ]` 中填入 `x` 变成 `[x]`，GitHub 会自动渲染为已勾选。  
> 代码完成后提交到仓库并通知我检查，我会给出面试官视角的点评。

---

## 项目结构

```
.
├── CMakeLists.txt          # 根构建文件，定义通用编译函数
├── tests/
│   └── test_utils.h        # Google Test 测试框架（已被 GTest 替代）
│                          # 运行测试: ctest 或 ./build/week2/dayXX_name_test
├── weekN/
│   ├── CMakeLists.txt
│   ├── dayXX_xxx.h         # 接口声明（模板类含 TODO 骨架）
│   ├── dayXX_xxx.cpp       # 实现骨架（非模板类，用户填写）
│   ├── dayXX_xxx_test.cpp  # 预置测试（完善后验证功能）
│   └── ...
├── docs/
│   └── weekN_code_review.md
└── build/
```

### 两种练习模式

| 模式 | 适用周 | 特点 |
|------|--------|------|
| **实现即运行** (Week 1) | 3-8 周 | 单 `.cpp` 自包含，含 `main()`，直接编译运行 |
| **测试驱动** (Week 2+) | 2 周起 | 接口 `.h` + 实现 `.cpp` + 独立测试 `.cpp` |

---

## 测试驱动工作流

从第 2 周开始，采用"声明 + 实现 + 测试"分离的模式：

### 每天的文件结构

- **`.h` 文件** — 接口声明 + TODO 标记，说明需要实现哪些函数
- **`.cpp` 文件** — 实现骨架，用户填写具体逻辑
- **`_test.cpp` 文件** — 预置的完整测试套件，覆盖正常/边界/异常场景

### 你的工作流

```
1. 阅读 .h 文件            ← 了解需要实现的接口
2. 尝试编译 test            ← 链接失败，看到"未实现"清单
3. 在 .cpp 或 .h 中实现     ← 填写 TODO 代码
4. 重新编译并运行 test      ← 验证实现是否正确
5. 绿色通过 → 打卡完成      ← 所有 GTest 测试通过
```

### 示例（Day01 — FileReader）

```bash
# 1. 尝试编译测试（预期：编译成功但部分测试失败）
cd build && cmake .. && make day01_file_reader_test
./week2/day01_file_reader_test
# → 输出部分 FAIL（因为骨架函数是空的）

# 2. 在 day01_file_reader.cpp 中实现 FileReader

# 3. 重新编译并运行
make day01_file_reader_test && ./week2/day01_file_reader_test
# → 全部 PASS
```

### 模板类的特殊情况

对于 `ScopedMutex`、`unique_ptr`、`shared_ptr` 等模板类：
- 实现直接写在 `.h` 文件中（模板必须）
- `.cpp` 文件仅起占位作用
- 编译测试时会出现 `undefined reference` 错误，指明哪些函数未实现

---

## 测试框架

**Google Test** 提供以下断言宏：

| 宏 | 用途 |
|----|------|
| `EXPECT_TRUE(cond)` / `ASSERT_TRUE(cond)` | 检查条件是否成立 |
| `EXPECT_EQ(a, b)` / `ASSERT_EQ(a, b)` | 检查两个值是否相等 |
| `EXPECT_THROW(expr, ExType)` | 检查表达式是否抛出指定异常 |
| `EXPECT_NO_THROW(expr)` | 检查表达式不抛异常 |
| `TEST(Suite, Name)` | 定义一个测试用例 |

所有测试输出统一由 GTest 管理，运行测试直接执行 `./build/week2/dayXX_name_test`，
或通过 `ctest` 批量运行。

---

## 📅 第一部分：8 周代码组织能力提升计划

### 第 1 周：基础封装 —— 消除全局变量
- [x] **周一**：手写 `Counter` 类，`increment()` / `decrement()` / `get()`，数据 `private`
- [x] **周二**：给 `Counter` 添加 `std::mutex` 成员，实现线程安全，接口不变
- [x] **周三**：手写 `ThreadSafeQueue<int>`，内部 `std::queue` + `std::mutex`
- [x] **周四**：添加 `wait_and_pop()`，使用 `std::condition_variable`
- [x] **周五**：用自己写的队列重写生产者-消费者，`main()` 不超过 30 行，无裸 `new`/`delete`
> **检验标准**：无全局变量、调用方无直接加锁/解锁
>
> 📖 **代码审查报告**：[docs/week1_code_review.md](docs/week1_code_review.md)

### 第 2 周：RAII 与资源管理
- [x] **周一**：手写 `FileReader`，构造打开文件，析构自动 `close`
- [x] **周二**：手写 `ScopedMutex`（类似 `std::lock_guard`），构造加锁，析构解锁
- [x] **周三**：手写极简 `unique_ptr<T>`，支持移动，禁止拷贝
- [x] **周四**：手写极简 `shared_ptr<T>`，带引用计数
- [x] **周五**：用 `unique_ptr` 改写链表，零 `delete` 关键字
> **检验标准**：资源生命周期与对象绑定，无手动释放
>
> 🧪 **测试验证**：`make dayXX_name_test && ./build/week2/dayXX_name_test`

### 第 3 周：生产者 - 消费者专题（高频手撕）
- [ ] **周一**：互斥锁 + 条件变量版（支持优雅退出 `stop()` + `notify_all()`）
- [ ] **周二**：信号量版（C++20 `std::counting_semaphore`）
- [ ] **周三**：无锁队列版（SPSC，`std::atomic` + 环形缓冲区）
- [ ] **周四**：多生产者多消费者版（正确同步）
- [ ] **周五**：有界队列版（满时生产者阻塞）
> **检验标准**：线程安全，无死锁，可自动退出

### 第 4 周：线程池
- [ ] **周一至周二**：固定线程数 `ThreadPool`，`submit` 返回 `std::future`
- [ ] **周三至周四**：用 `std::priority_queue` 实现任务优先级
- [ ] **周五**：实现优雅退出（等待所有任务完成再析构线程）
> **检验标准**：10 行内启动线程池并获取结果

### 第 5 周：状态机编程
- [ ] **周一至周二**：TCP 三次握手状态机（CLOSED / SYN_SENT / ESTABLISHED）
- [ ] **周三至周四**：HTTP 请求解析器，逐字节驱动状态转移
- [ ] **周五**：复盘简历 HTTP Server 项目的解析状态机
> **检验标准**：每个 `case` 不超过 10 行，状态图清晰

### 第 6 周：观察者模式与回调
- [ ] **周一至周二**：纯虚接口版 `Subject` / `Observer`，通知时遍历列表
- [ ] **周三至周四**：用 `std::function` 实现回调版，无侵入
- [ ] **周五**：解释 Qt 信号槽是观察者模式的进化，结合文档再编辑项目
> **检验标准**：头文件无相互引用

### 第 7 周：单例与工厂
- [ ] **周一至周二**：手写 Meyers' Singleton（线程安全），解释 C++11 保证
- [ ] **周三至周四**：手写工厂模式，`map<string, function<Base*()>>` 注册产品
- [ ] **周五**：复盘文档格式化项目中的 Word/Excel/PPT 导出工厂
> **检验标准**：单例 10 行内，工厂一行注册

### 第 8 周：综合模拟面试
- [ ] 自选一题闭卷手写：内存池 / LRU Cache / 无锁队列
- [ ] 录制自己讲解设计决策，检查表达是否流畅
> **检验标准**：30 分钟内无结构性问题，能清晰阐述取舍原因

> 💡 **周次说明**：第 3 周起延续 Week 1 的"实现即运行"模式（单 `.cpp`），
> 第 2 周的"头文件 + 实现 + 测试"分离模式仅用于引入 RAII 这一核心概念时
> 提供更严谨的验证。

---

## 🧠 第二部分：C++ 核心知识背诵卡

### 对象模型
- **虚函数表**：每个含虚函数的类有一张表（存放在只读数据段），对象首部存 vptr
- **多继承**：多个 vptr，`B* b = c` 指针可能偏移 `sizeof(A)`（**你上次答错，务必牢记**）
- **虚继承**：用 vbptr 指向虚基类表，消除菱形继承的冗余
- **构造/析构中的虚函数**：调用当前类的版本，**不具备多态性**（析构也一样）
- **`dynamic_cast`**：依赖 vtable 中的 `type_info`，运行时检查，失败返回 `nullptr` 或抛异常
- **内存对齐**：成员按照自身大小对齐，类大小是最宽成员的整数倍

### STL 核心
- **`vector` 扩容**：GCC 2 倍，VS 1.5 倍；`push_back` 多一次临时对象构造，优先用 `emplace_back`
- **`map` vs `unordered_map`**：红黑树（有序）vs 哈希表（O(1) 查找）
- **迭代器失效**：`vector` 插入/删除可能导致全部失效；`list`/`map` 只有被删除元素失效

### 智能指针
- **`shared_ptr`**：两个指针（对象 + 控制块），引用计数线程安全，对象本身不是
- **`weak_ptr`**：打破循环引用，`lock()` 返回临时 `shared_ptr`，对象销毁后返回空
- **`make_shared`**：一次分配对象和控制块，高效；`new shared_ptr` 两次分配
- **`enable_shared_from_this`**：安全获取 `this` 的 `shared_ptr`

### 模板元编程（支撑你的 XML 反射项目）
- **SFINAE**：替换失败不是错误，匹配重载时淘汰无效版本
- **完美转发**：`T&&` + `std::forward<T>` 保留左右值属性
- **`std::is_polymorphic`**：利用 `dynamic_cast<void*>` 只能用于多态类的规则检测

### 编译与链接
- **编译四步**：预处理 → 编译 → 汇编 → 链接
- **动态库 PLT/GOT**：延迟绑定，首次调用时解析地址填表
- **符号可见性**：`-fvisibility=hidden` 默认隐藏，`__attribute__((visibility("default")))` 导出，可减小 SO 体积 30%+
- **`extern "C"`**：阻止 Name Mangling，JNI 和动态库导出必须使用

### 并发与网络
- **`volatile` vs `atomic`**：`volatile` 防编译器优化，不保证原子性和内存序；`atomic` 保证两者
- **Epoll ET**：必须循环读直到 `EAGAIN`，配合非阻塞 IO
- **Reactor vs Proactor**：前者同步非阻塞（epoll），后者异步 IO（IOCP/io_uring）
- **零拷贝**：`sendfile` 减少上下文切换和 CPU 拷贝
- **TIME_WAIT**：主动关闭方等待 2MSL，确保最后的 ACK 能到达，用 `SO_REUSEADDR` 缓解

### 设计模式
- **RAII**：资源获取即初始化，智能指针、`lock_guard` 都是实例
- **单例**：Meyers' Singleton，C++11 `static` 局部变量线程安全
- **工厂**：`map` 注册创建函数，扩展新产品不改现有代码
- **观察者**：Qt 信号槽、回调函数都是其变体

---

## 📌 使用说明

1. **打卡**：每完成当天练习，修改前面的 `[ ]` 为 `[x]` 并提交到 GitHub。
2. **测试驱动**：从第 2 周起，完成实现后运行对应的 `_test` 目标验证正确性。
3. **提交检查**：练习代码 push 后通知我，我会逐行点评并给出改进建议。
4. **复习**：面试前 30 分钟，快速浏览第二部分背诵卡的加粗结论。
5. **记录**：遇到新的面试题或知识点，随时补充到对应章节。

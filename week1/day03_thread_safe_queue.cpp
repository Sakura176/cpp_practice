#include <initializer_list>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

template<typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(std::initializer_list<T>&& que) : m_que(que) {}
    void push(T& val)
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.push(val);
    }

    void pop()
    {
        std::lock_guard<std::mutex> l(mtx);
        m_que.pop();
    }

    T front() const
    {
        // std::lock_guard<std::mutex> l(mtx);
        return m_que.front();
    }

    bool empty() const { return m_que.empty(); }

private:
    std::mutex    mtx;
    std::queue<T> m_que;
};

int main(int argc, char* argv[])
{
    ThreadSafeQueue<int> que({1, 2, 3, 4, 5});

    auto func_push = [&que]() {
        int cnt = 0;
        while (true) {
            que.push(cnt);
            std::cout << "push val: " << cnt << std::endl;
            cnt++;
        }
    };

    auto func_pop = [&que]() {
        while (true) {
            if (!que.empty()) {
                int val = que.front();
                que.pop();
                std::cout << "pop val: " << val << std::endl;
            }
        }
    };

    std::thread thread_push(func_push);
    std::thread thread_pop(func_pop);

    thread_push.join();
    thread_pop.join();

    return 0;
}

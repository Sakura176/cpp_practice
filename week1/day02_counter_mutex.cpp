#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

class Counter {
public:
  void incresment() {
    std::lock_guard<std::mutex> lock(mtx);
    count++;
  }

  void decresment() {
    std::lock_guard<std::mutex> lock(mtx);
    count--;
  }

  long get() const { return count; }

private:
  long count{0};
  std::mutex mtx;
};

int main(int argc, char *argv[]) {
  Counter count;

  auto incres_func = [&count]() {
    while (true) {
      // std::this_thread::sleep_for(1s);
      count.incresment();

      std::cout << "incresment count: " << count.get() << std::endl;
    }
  };
  auto decres_func = [&count]() {
    while (true) {
      // std::this_thread::sleep_for(2s);
      count.decresment();

      std::cout << "decresment count: " << count.get() << std::endl;
    }
  };

  std::thread incre_thread(incres_func);
  std::thread decre_thread(decres_func);

  incre_thread.join();
  decre_thread.join();
  std::cout << "end" << count.get() << std::endl;
  return 0;
}

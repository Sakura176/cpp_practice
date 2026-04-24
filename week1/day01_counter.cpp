#include <iostream>

class Counter {
public:
  void incresment() { count++; }

  void decresment() { count--; }
  long get() const { return count; }

private:
  long count{0};
};

int main(int argc, char *argv[]) {
  Counter count;
  count.incresment();
  std::cout << "count: " << count.get() << std::endl;
  count.decresment();
  std::cout << "count: " << count.get() << std::endl;
  return 0;
}

#include "arc.h"

// Example usage
struct MyData {
  int value;

  MyData(int val) : value(val) {}
};

int main() {
  auto arc = Arc<MyData>(new MyData(42));

  // Cloning
  auto arc2 = arc.clone();

  // Modifying data through interior mutability
  {
    MyData *data = get_mut(arc2);
    data->value = 99;
  }

  // Accessing data concurrently
  std::thread thread1([&arc]() {
    auto localArc = arc;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::shared_lock<std::shared_mutex> lock(*localArc.mutex());
    std::cout << "Thread 1: " << localArc.get()->value << std::endl;
  });

  std::thread thread2([&arc]() {
    auto localArc = arc;
    std::shared_lock<std::shared_mutex> lock(*localArc.mutex());
    std::cout << "Thread 2: " << localArc.get()->value << std::endl;
  });

  thread1.join();
  thread2.join();

  auto weakArc = WeakArc<MyData>(arc);
  auto upgradedArc = weakArc.upgrade();
  if (upgradedArc.get() != nullptr) {
    std::shared_lock<std::shared_mutex> lock(*upgradedArc.mutex());
    std::cout << "Upgraded Arc: " << upgradedArc.get()->value << std::endl;
  } else {
    std::cout << "Weak Arc has expired" << std::endl;
  }

  return 0;
}

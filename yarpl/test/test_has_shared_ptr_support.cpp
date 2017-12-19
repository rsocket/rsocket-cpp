#include <atomic>
#include <memory>

int main() {
  std::shared_ptr<int> i;
  auto il = std::atomic_load(&i);
  return 0;
}

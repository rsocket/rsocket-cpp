#include <atomic>
#include <memory>

int main() {
  std::atomic<std::shared_ptr<int>> i;
  std::shared_ptr<int> j;
  std::atomic_store(&i, j);
  return 0;
}

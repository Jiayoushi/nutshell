#include <iostream>
#include <functional>
#include <future>
#include <vector>
#include <thread>
#include <cmath>
#include <cassert>

#include "concurrent_queue.h"

void test_concurrent_queue() {
  ConcurrentQueue<int> cq;

  auto work = 
    [&cq](uint64_t total_task) {
      uint64_t sum = 0;
      for (int i = 0; i < total_task; ++i) {
        std::shared_ptr<int> val;
        do {
          val = cq.try_pop();

          if (val == nullptr) {
            std::this_thread::yield();
            continue;
          }
        } while (val == nullptr);

        sum += *val;
      }
      return sum;
    };

  std::vector<std::future<uint64_t>> futures;
  const uint64_t kWorkPerThread = 1000000;
  const int kTotalThreads = 10;
  const uint64_t kTotalWork = kWorkPerThread * kTotalThreads;
  for (int i = 0; i < kTotalThreads; ++i) {
    futures.emplace_back(std::move(std::async(work, kWorkPerThread)));
  }

  for (int i = 1; i <= kTotalWork; ++i) {
    cq.push(i);
  }

  uint64_t sum = 0;
  for (int i = 0; i < kTotalThreads; ++i) {
    sum += futures[i].get();
  }
  const uint64_t kExpcetedSum = (1 + kTotalWork) * (kTotalWork / 2);

  assert(sum == kExpcetedSum);
}

int main() {
  test_concurrent_queue();


  return 0;
}
#pragma once
#include <mutex>
#include <queue>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/eventfd.h>

//------------------------------------------------------------------------------

template<typename T>
struct ThreadQueue {

  std::queue<T> queue_;
  std::mutex mut_;
  int fd = 0;

  ThreadQueue() {
    fd = eventfd(0, EFD_SEMAPHORE);
  }

  void put(T buf) {
    {
      std::lock_guard<std::mutex> lock(mut_);
      queue_.push(std::move(buf));
    }
    uint64_t inc = 1;
    auto _ = write(fd, &inc, 8);
  }

  T get() {
    uint64_t inc = 0;
    auto _ = read(fd, &inc, 8);
    assert(inc == 1);
    std::lock_guard<std::mutex> lock(mut_);
    auto buf = std::move(queue_.front());
    queue_.pop();
    return buf;
  }

  [[nodiscard]] bool empty() {
    std::lock_guard<std::mutex> lock(mut_);
    return queue_.empty();
  }

  [[nodiscard]] size_t count() {
    std::lock_guard<std::mutex> lock(mut_);
    return queue_.size();
  }
};

//------------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

struct ThreadPool {
public:
  std::vector<std::thread> threads;
  std::mutex mutex;
  std::atomic<bool> stop{false};
  std::queue<std::function<void()>> tasks;
  std::condition_variable condition;

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
    if (num_threads == 0)
      num_threads = 1;
    for (size_t i = 0; i < num_threads; ++i) {
      threads.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock,
                           [this] { return stop.load() || !tasks.empty(); });
            if (stop.load() && tasks.empty())
              return;
            task = std::move(tasks.front());
            tasks.pop();
          }
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(mutex);
      stop.store(true);
    }
    condition.notify_all();
    for (auto &thread : threads) {
      if (thread.joinable())
        thread.join();
    }
  }

  void enqueue(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (stop.load())
        return;
      tasks.emplace(std::move(task));
    }
    condition.notify_one();
  }
};

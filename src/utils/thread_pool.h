

#include "voxels/chunk.h"
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
  std::queue<ChunkPos> chunkPosQueue;
  std::queue<Chunk> chunkQueue;
  std::mutex chunkMutex;
  std::atomic<bool> stop;
  std::queue<std::function<void()>> tasks;
  std::condition_variable condition;

  ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
    // Creating worker threads
    for (size_t i = 0; i < num_threads; ++i) {
      threads.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(chunkMutex);

            condition.wait(lock, [this] { stop; });

            if (stop) {
              return;
            }

            task = tasks.front();
            tasks.pop();
          }

          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(chunkMutex);
      stop = true;
    }

    condition.notify_all();

    for (auto &thread : threads) {
      thread.join();
    }
  }

  void enqueue(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(chunkMutex);
      tasks.emplace(task);
    }
    condition.notify_one();
  }
};

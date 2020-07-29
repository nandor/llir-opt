// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <atomic>
#include <thread>



/**
 * Parallel job runner which requests jobs until one finishes.
 */
template <typename Task, typename Result>
class JobRunner {
public:
  /// Initialises the job runner.
  JobRunner(unsigned threadCount = 1) : threadCount_(threadCount) {}

  /// Cleanup on exit.
  virtual ~JobRunner()
  {
  }

  /// Run the pool.
  void Execute()
  {
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < threadCount_; ++i) {
      threads.emplace_back([this] {
        while (true) {
          // Get a new task to execute.
          Task task;
          {
            std::lock_guard<std::mutex> guard(lock_);
            if (auto newTask = Request()) {
              task = std::move(*newTask);
            } else {
              return;
            }
          }

          // Run the task.
          Result result = Run(std::move(task));

          // Post the result.
          {
            std::lock_guard<std::mutex> guard(lock_);
            Post(std::move(result));
          }
        }
      });
    }

    for (std::thread &thread : threads) {
      thread.join();
    }
  }

protected:
  /// Request a new task to run on a free thread.
  /// Short-running task that runs on the main thread.
  virtual std::optional<Task> Request() = 0;

  /// Run the task on a separate thread.
  virtual Result Run(Task &&task) = 0;

  /// Post the result of a task.
  virtual void Post(Result &&result) = 0;

private:
  /// Number of threads to run.
  unsigned threadCount_;
  /// Synchronisation between threads.
  std::mutex lock_;
};

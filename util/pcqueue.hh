#ifndef UTIL_PCQUEUE_H
#define UTIL_PCQUEUE_H

#include "exception.hh"

#include <condition_variable>
#include <memory>
#include <mutex>

#include <cerrno>

namespace util {

// C++11 condition_variable based semaphore (portable, no boost or Mach deps)
class Semaphore {
  public:
    explicit Semaphore(int value) : count_(value) {}

    void wait() {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return count_ > 0; });
      --count_;
    }

    void post() {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        ++count_;
      }
      cv_.notify_one();
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

inline void WaitSemaphore(Semaphore &semaphore) {
  while (1) {
    try {
      semaphore.wait();
      break;
    }
    catch (const std::exception &) {
      // retry on any transient issues
    }
  }
}

/**
 * Producer consumer queue safe for multiple producers and multiple consumers.
 * T must be default constructable and have operator=.
 * The value is copied twice for Consume(T &out) or three times for Consume(),
 * so larger objects should be passed via pointer.
 * Strong exception guarantee if operator= throws.  Undefined if semaphores throw.
 */
template <class T> class PCQueue {
 public:
  explicit PCQueue(size_t size)
   : empty_(size), used_(0),
     storage_(new T[size]),
     end_(storage_.get() + size),
     produce_at_(storage_.get()),
     consume_at_(storage_.get()) {}

  // Add a value to the queue.
  void Produce(const T &val) {
    WaitSemaphore(empty_);
    {
      std::unique_lock<std::mutex> produce_lock(produce_at_mutex_);
      try {
        *produce_at_ = val;
      }
      catch (...) {
        empty_.post();
        throw;
      }
      if (++produce_at_ == end_) produce_at_ = storage_.get();
    }
    used_.post();
  }

  // Consume a value, assigning it to out.
  T& Consume(T &out) {
    WaitSemaphore(used_);
    {
      std::unique_lock<std::mutex> consume_lock(consume_at_mutex_);
      try {
        out = *consume_at_;
      }
      catch (...) {
        used_.post();
        throw;
      }
      if (++consume_at_ == end_) consume_at_ = storage_.get();
    }
    empty_.post();
    return out;
  }

  // Convenience version of Consume that copies the value to return.
  // The other version is faster.
  T Consume() {
    T ret;
    Consume(ret);
    return ret;
  }

 private:
  PCQueue(const PCQueue &) = delete;
  PCQueue &operator=(const PCQueue &) = delete;

  // Number of empty spaces in storage_.
  Semaphore empty_;
  // Number of occupied spaces in storage_.
  Semaphore used_;

  std::unique_ptr<T[]> storage_;

  T *const end_;

  // Index for next write in storage_.
  T *produce_at_;
  std::mutex produce_at_mutex_;

  // Index for next read from storage_.
  T *consume_at_;
  std::mutex consume_at_mutex_;

};

} // namespace util

#endif // UTIL_PCQUEUE_H

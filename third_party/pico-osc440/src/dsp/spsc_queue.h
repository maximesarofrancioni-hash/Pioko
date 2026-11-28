#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace dsp {

// Lock-free single-producer, single-consumer ring buffer. One side only push()es,
// the other only pop()s; they may run on different cores. N must be a power of 2
// (holds N - 1 items). On the RP2350 the atomics compile to plain loads/stores
// with memory barriers: no locks, no interrupts disabled.
template <class T, size_t N>
class SpscQueue {
  static_assert((N & (N - 1)) == 0, "N must be a power of 2");

 public:
  bool push(const T& item) {
    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t next = (head + 1) & (N - 1);
    if (next == tail_.load(std::memory_order_acquire)) return false;   // full
    items_[head] = item;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool empty() const { return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire); }

  bool pop(T& item) {
    const uint32_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false;   // empty
    item = items_[tail];
    tail_.store((tail + 1) & (N - 1), std::memory_order_release);
    return true;
  }

 private:
  T items_[N];
  std::atomic<uint32_t> head_{0};   // written by the producer only
  std::atomic<uint32_t> tail_{0};   // written by the consumer only
};

}  // namespace dsp

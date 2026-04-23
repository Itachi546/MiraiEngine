#pragma once

#include <mutex>

namespace mirai {
    template <typename T, size_t capacity>
    class ThreadSafeRingBuffer {
      public:
        // Push an item to the end if there is free space
        //	Returns true if succesful
        //	Returns false if there is not enough space
        inline bool push_back(const T &item) {
            std::lock_guard<std::mutex> lk(mu);

            size_t next = (head + 1) % capacity;
            if (next != tail) {
                data[head] = item;
                head = next;
                return true;
            }
            return false;
        }

        // Get an item if there are any
        //	Returns true if succesful
        //	Returns false if there are no items
        inline bool pop_front(T &item) {
            std::lock_guard<std::mutex> lk(mu);
            if (tail != head) {
                item = data[tail];
                tail = (tail + 1) % capacity;
                return true;
            }
            return false;
        }

        // Returns true if the queue has no items
        inline bool empty() {
            std::lock_guard<std::mutex> lk(mu);
            return tail == head;
        }

      private:
        T data[capacity];
        size_t head = 0;
        size_t tail = 0;
        std::mutex mu;
    };
} // namespace mirai
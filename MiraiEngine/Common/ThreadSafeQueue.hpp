#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue
{
  public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue &other)
    {
        std::lock_guard<std::mutex> lk(other.mutex);
        this->data = other->data;
    }

    ThreadSafeQueue &operator=(const ThreadSafeQueue &other) = delete;

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mutex);
        return data.size() == 0;
    }

    void push(T entry)
    {
        std::lock_guard<std::mutex> lk(mutex);
        data.push(entry);
        data_cond.notify_one();
    }

    bool try_pop(T &val)
    {
        std::lock_guard<std::mutex> lk(mutex);
        if (data.empty())
            return false;
        val = data.front();
        data.pop();
        return true;
    }

    bool wait_and_pop(T &val)
    {
        std::unique_lock<std::mutex> lk(mutex);
        data_cond.wait(lk, [this]()
                       { return !data.empty(); });
        val = data.front();
        data.pop();
        return true;
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::lock_guard<std::mutex> lk(mutex);
        data_cond.wait(lk, [this]()
                       { return !data.empty(); });
        std::shared_ptr<T> result(std::make_shared<T>(data.front()));
        data.pop();
        return result;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mutex);
        if (data.empty())
            return nullptr;
        std::shared_ptr<T> result(std::make_shared<T>(data.front()));
        data.pop();
        return result;
    }

  private:
    std::queue<T> data;
    mutable std::mutex mutex;
    std::condition_variable data_cond;
};
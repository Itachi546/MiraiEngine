#pragma once

#include <array>

template <typename T, std::size_t N>
class RingBuffer
{
    RingBuffer() : current_index(0)
    {
    }

    std::size_t capacity()
    {
        return N;
    }

    

    T &next()
    {
        uint32_t next_index = (current_index + 1) % N;
        return buffer[next_index];
    }

  private:
    T buffer[N];
    uint32_t head, tail;
};
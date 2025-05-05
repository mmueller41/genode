#pragma once

#include <cstdint>
#include <cstdlib>

namespace mx::queue
{
template<typename T>
class DynamicRingpuffer
{
public:
    DynamicRingpuffer()
    {
        _data = std::aligned_alloc(64U, sizeof(T*) * _capacity);
    }

    ~DynamicRingpuffer()
    {
        std::free(_data);
    }

    void push_back(T* item)
    {
        const auto index = (_head++) & _capacity;
        if (index == _tail)
        {
            /// TODO: Reallocate
        }
        _data[index] = item;
    }
private:
    T **_data;
    std::uint64_t _capacity {1024U};
    std::uint64_t _head {0U};
    std::uint64_t _tail {0U};


};
}
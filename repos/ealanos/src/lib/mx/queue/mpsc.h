#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mx/system/cache.h>
#include <utility>

namespace mx::queue {
/**
 * Multi producer, single consumer queue with unlimited slots.
 * Every thread can push values into the queue without using latches.
 *
 * Inspired by http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
 */
template <class T> class MPSC
{
public:
    constexpr MPSC() noexcept
        : _head(reinterpret_cast<T *>(_stub.data())), _tail(reinterpret_cast<T *>(_stub.data())),
          _end(reinterpret_cast<T *>(_stub.data()))
    {
    }
    ~MPSC() noexcept = default;

    /**
     * Inserts the given item into the queue.
     * @param item Item to insert.
     */
    void push_back(T *item) noexcept
    {
        item->next(nullptr);
        auto *prev = __atomic_exchange_n(&_head, item, __ATOMIC_RELAXED);
        prev->next(item);
    }

    /**
     * Inserts all items between begin and end into the queue.
     * Items must be linked among themselves.
     * @param begin First item to insert.
     * @param end Last item to insert.
     */
    void push_back(T *begin, T *end) noexcept
    {
        end->next(nullptr);
        auto *old_head = __atomic_exchange_n(&_head, end, __ATOMIC_RELAXED);
        old_head->next(begin);
    }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] const T *end() const noexcept { return _end; }

    /**
     * @return True, when the queue is empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return _tail == _end && reinterpret_cast<T const &>(_stub).next() == nullptr;
    }

    /**
     * @return Takes and removes the first item from the queue.
     */
    [[nodiscard]] T *pop_front() noexcept;

    /**
     * Pops all items from the list. The items will be concatenated.
     * This operation is NOT thread safe.
     *
     * @return Pair of first and last task
     */
    [[nodiscard]] std::pair<T *, T *> pop() noexcept;

private:
    // Head of the queue (accessed by every producer).
    alignas(64) T *_head;

    // Tail of the queue (accessed by the consumer and producers if queue is empty)-
    alignas(64) T *_tail;

    // Pointer to the end.
    alignas(16) T *const _end;

    // Dummy item for empty queue.
    alignas(64) std::array<std::byte, sizeof(T)> _stub = {};
};

template <class T> T *MPSC<T>::pop_front() noexcept
{
    auto *tail = this->_tail;
    auto *next = tail->next();

    if (tail == this->_end)
    {
        if (next == nullptr)
        {
            return nullptr;
        }

        this->_tail = next;
        tail = next;
        next = next->next();
    }

    if (next != nullptr)
    {
        this->_tail = next;
        return tail;
    }

    const auto *head = this->_head;
    if (tail != head)
    {
        return nullptr;
    }

    this->push_back(this->_end);

    next = tail->next();
    if (next != nullptr)
    {
        this->_tail = next;
        return tail;
    }

    return nullptr;
}
template <class T> std::pair<T *, T *> MPSC<T>::pop() noexcept
{
    T *head = nullptr;
    /// Head and tail are interchanged, head is tail and tail is head.
    if (this->_tail != nullptr)
    {
        if (this->_tail != this->_end)
        {
            head = this->_tail;
        }
        else
        {
            head = this->_tail->next();
        }
    }

    if (head == nullptr)
    {
        return std::make_pair(nullptr, nullptr);
    }

    if (this->_head == nullptr || this->_head == this->_end)
    {
        head->next(nullptr);
        return std::make_pair(head, nullptr);
    }

    auto *tail = std::exchange(this->_head, this->_end);
    tail->next(nullptr);
    this->_tail = this->_end;

    return std::make_pair(head, tail);
}

} // namespace mx::queue
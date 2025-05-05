#pragma once

#include <utility>

namespace mx::queue {
/**
 * Single producer and consumer queue. This queue is not thread safe.
 */
template <class T> class alignas(64) List
{
public:
    constexpr List() noexcept = default;
    ~List() noexcept = default;

    /**
     * Inserts an item into the queue.
     * @param item Item to be inserted.
     */
    void push_back(T *item) noexcept
    {
        item->next(nullptr);

        if (_tail != nullptr) [[likely]]
        {
            _tail->next(item);
            _tail = item;
        }
        else
        {
            _head = _tail = item;
        }
    }

    /**
     * Inserts a list of items into the queue.
     * The items have to be concatenated.
     *
     * @param first First item to be inserted.
     * @param last Last item to be inserted.
     */
    void push_back(T *first, T *last) noexcept
    {
        last->next(nullptr);

        if (_tail != nullptr) [[likely]]
        {
            _tail->next(first);
            _tail = last;
        }
        else
        {
            _head = first;
            _tail = last;
        }
    }

    /**
     * @return Begin of the queue.
     */
    [[nodiscard]] T *begin() noexcept { return _head; }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] const T *end() const noexcept { return _tail; }

    /**
     * @return End of the queue.
     */
    [[nodiscard]] T *end() noexcept { return _tail; }

    /**
     * @return True, when the queue is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _head == nullptr; }

    /**
     * @return Takes and removes the first item from the queue.
     */
    T *pop_front() noexcept
    {
        if (_head == nullptr) [[unlikely]]
        {
            return nullptr;
        }

        auto *head = _head;
        auto *new_head = head->next();
        if (new_head == nullptr) [[unlikely]]
        {
            _tail = nullptr;
        }

        _head = new_head;
        return head;
    }

    std::pair<T *, std::uint16_t> pop_front(const std::uint16_t limit) noexcept
    {
        auto count = 0U;
        auto *head = _head;
        auto *current = _head;
        do
        {
            current = current->next();
            ++count;
        } while (count < limit && current != nullptr);

        _head = current;
        if (current == nullptr) [[unlikely]]
        {
            _tail = nullptr;
        }

        return std::make_pair(head, count);
    }

    /**
     * Pops all items from the list. The items will be concatenated.
     *
     * @return Pair of first and last task
     */
    [[nodiscard]] std::pair<T *, T *> pop() noexcept
    {
        if (_head == nullptr)
        {
            return std::make_pair(nullptr, nullptr);
        }

        if (_head == _tail)
        {
            auto *head = _head;
            _head = _tail = nullptr;
            return std::make_pair(head, nullptr);
        }

        auto *head = std::exchange(_head, nullptr);
        auto *tail = std::exchange(_tail, nullptr);
        return std::make_pair(head, tail);
    }

private:
    // Pointer to the head.
    T *_head{nullptr};

    // Pointer to the tail.
    T *_tail{nullptr};
};
} // namespace mx::queue
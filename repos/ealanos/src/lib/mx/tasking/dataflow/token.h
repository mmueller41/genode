#pragma once

#include <mx/tasking/annotation.h>

namespace mx::tasking::dataflow {
template <typename T> class Token
{
public:
    Token() noexcept = default;
    explicit Token(T &&data) noexcept : _data(std::move(data)) {}
    explicit Token(const T &data) noexcept : _data(data) {}
    Token(T &&data, tasking::annotation annotation) noexcept
        : _data(std::move(data)), _annotation(std::move(annotation))
    {
    }
    Token(const T &data, tasking::annotation annotation) noexcept : _data(data), _annotation(std::move(annotation)) {}
    Token(Token<T> &&other) noexcept : _data(std::move(other._data)), _annotation(std::move(other._annotation)) {}
    ~Token() noexcept = default;

    Token<T> &operator=(Token<T> &&other) noexcept
    {
        _data = std::move(other._data);
        _annotation = other._annotation;
        return *this;
    }

    [[nodiscard]] const T &data() const noexcept { return _data; }
    [[nodiscard]] T &data() noexcept { return _data; }
    [[nodiscard]] tasking::annotation annotation() const noexcept { return _annotation; }
    [[nodiscard]] tasking::annotation &annotation() noexcept { return _annotation; }

private:
    T _data;
    class tasking::annotation _annotation
    {
    };
};

template <typename T> static inline Token<T> make_token(T &&data) noexcept
{
    return Token<T>{std::move(data)};
}

template <typename T> static inline Token<T> make_token(const T &data) noexcept
{
    return Token<T>{data};
}

template <typename T> static inline Token<T> make_token(T &&data, tasking::annotation annotation) noexcept
{
    return Token<T>{std::move(data), annotation};
}

template <typename T> static inline Token<T> make_token(const T &data, tasking::annotation annotation) noexcept
{
    return Token<T>{data, annotation};
}
} // namespace mx::tasking::dataflow
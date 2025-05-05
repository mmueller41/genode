#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <mx/memory/alignment_helper.h>
#include <mx/resource/ptr.h>
#include <mx/system/builtin.h>
#include <mx/system/environment.h>
#include <sstream>
#include <string>
#include <utility>

namespace mx::tasking {

class PrefetchDescriptor
{
public:
    using data_t = std::uint64_t;

    enum ExecuteType : std::uint8_t
    {
        Size = 0b01,
        Callback = 0b10,
        Mask = 0b11,
    };

    enum PrefetchType : std::uint8_t
    {
        Temporal = 0b01,
        NonTemporal = 0b10,
        Write = 0b11,
    };

    enum Type : std::uint8_t
    {
        None = 0b0000,

        SizeTemporal = (ExecuteType::Size << 2U) | PrefetchType::Temporal,
        SizeNonTemporal = (ExecuteType::Size << 2U) | PrefetchType::NonTemporal,
        SizeWrite = (ExecuteType::Size << 2U) | PrefetchType::Write,

        CallbackAny = ExecuteType::Callback << 2U,

        MaskTemporal = (ExecuteType::Mask << 2U) | PrefetchType::Temporal,
        MaskNonTemporal = (ExecuteType::Mask << 2U) | PrefetchType::NonTemporal,
        MaskWrite = (ExecuteType::Mask << 2U) | PrefetchType::Write,
    };

private:
    static inline constexpr auto BITS = sizeof(data_t) * 8U;
    static inline constexpr auto RESERVED_BITS = 4U;
    static inline constexpr auto DATA_BITS = BITS - RESERVED_BITS;
    static inline constexpr auto CLEAR_TYPE_MASK = std::numeric_limits<data_t>::max() >> RESERVED_BITS;

public:
    [[nodiscard]] static constexpr auto capacity() { return DATA_BITS; }
    [[nodiscard]] static constexpr auto bits() { return BITS; }

    [[nodiscard]] static PrefetchDescriptor make_size(const PrefetchType type, const data_t data) noexcept
    {
        return PrefetchDescriptor{((data_t(ExecuteType::Size << 2U) | type) << DATA_BITS) | data};
    }

    [[nodiscard]] static PrefetchDescriptor make_mask(const PrefetchType type, const data_t data) noexcept
    {
        return PrefetchDescriptor{((data_t(ExecuteType::Mask << 2U) | type) << DATA_BITS) | data};
    }

    [[nodiscard]] static PrefetchDescriptor make_callback(const data_t data) noexcept
    {
        return PrefetchDescriptor{(data_t(ExecuteType::Callback << 2U) << DATA_BITS) | data};
    }

    constexpr PrefetchDescriptor() noexcept = default;
    constexpr explicit PrefetchDescriptor(const data_t data) noexcept : _data(data) {}
    ~PrefetchDescriptor() noexcept = default;

    /**
     * @return The type if the descriptor.
     */
    [[nodiscard]] Type id() const noexcept { return static_cast<Type>(_data >> DATA_BITS); }

    [[nodiscard]] bool empty() const noexcept { return (_data & CLEAR_TYPE_MASK) == 0U; }

    [[nodiscard]] data_t data() const noexcept { return _data; }
    [[nodiscard]] data_t &data() noexcept { return _data; }
    [[nodiscard]] data_t data_without_descriptor_bits() const noexcept { return _data & CLEAR_TYPE_MASK; }

    PrefetchDescriptor operator|(const PrefetchDescriptor other) const noexcept
    {
        return PrefetchDescriptor{_data | other._data};
    }
    PrefetchDescriptor &operator|=(const PrefetchDescriptor other) noexcept
    {
        _data |= other._data;
        return *this;
    }
    bool operator==(const PrefetchDescriptor other) const noexcept { return _data == other._data; }

private:
    data_t _data{0U};
};

class PrefetchSizeView
{
public:
    constexpr PrefetchSizeView(const PrefetchDescriptor::data_t data) noexcept : _data(data) {}
    PrefetchSizeView(const PrefetchDescriptor data) noexcept : _data(data.data_without_descriptor_bits()) {}
    ~PrefetchSizeView() noexcept = default;

    PrefetchSizeView &operator=(PrefetchSizeView &&) noexcept = default;
    PrefetchSizeView &operator=(const PrefetchSizeView &) noexcept = default;

    /**
     * @return The size to prefetch.
     */
    [[nodiscard]] std::uint64_t get() const noexcept { return _data; }

private:
    PrefetchDescriptor::data_t _data;
};

class PrefetchMaskView
{
public:
    constexpr PrefetchMaskView(const PrefetchDescriptor::data_t data) noexcept : _data(data) {}
    PrefetchMaskView(const PrefetchDescriptor data) noexcept : _data(data.data_without_descriptor_bits()) {}
    ~PrefetchMaskView() noexcept = default;

    PrefetchMaskView &operator=(PrefetchMaskView &&) noexcept = default;
    PrefetchMaskView &operator=(const PrefetchMaskView &) noexcept = default;

    /**
     * @return Number of cache lines that can be stored within the mask.
     */
    [[nodiscard]] static constexpr auto capacity() { return PrefetchDescriptor::capacity(); }

    /**
     * @return Number of set bits.
     */
    [[nodiscard]] std::uint8_t count() const noexcept { return std::popcount(_data); }

    /**
     * @return True, if the data is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _data == 0U; }

    /**
     * Tests if a given index is set.
     *
     * @param index Index to test.
     * @return True, if the given index is set.
     */
    [[nodiscard]] bool test(const std::uint8_t index) const noexcept
    {
        return static_cast<bool>(_data & (PrefetchDescriptor::data_t{1U} << index));
    }

private:
    PrefetchDescriptor::data_t _data;
};

class PrefetchCallbackView
{
public:
    using callback_t = void (*)(void *);
    [[nodiscard]] static constexpr auto bits_for_size() { return 8U; }
    [[nodiscard]] static constexpr auto bits_for_pointer() { return PrefetchDescriptor::capacity() - bits_for_size(); }

    constexpr PrefetchCallbackView(const PrefetchDescriptor::data_t data) noexcept : _data(data) {}
    PrefetchCallbackView(const PrefetchDescriptor data) noexcept : _data(data.data_without_descriptor_bits()) {}
    ~PrefetchCallbackView() noexcept = default;

    PrefetchCallbackView &operator=(PrefetchCallbackView &&) noexcept = default;
    PrefetchCallbackView &operator=(const PrefetchCallbackView &) noexcept = default;

    /**
     * @return The number of cache lines that will be prefetched by the callback.
     */
    [[nodiscard]] std::uint8_t size() const noexcept { return _data >> bits_for_pointer(); }

    /**
     * @return The callback for prefetching.
     */
    [[nodiscard]] callback_t get() const noexcept
    {
        return reinterpret_cast<callback_t>(_data & PrefetchDescriptor::data_t(std::pow(2, bits_for_pointer()) - 1));
    }

private:
    PrefetchDescriptor::data_t _data;
};

class PrefetchSize
{
public:
    [[nodiscard]] static PrefetchDescriptor make(const PrefetchDescriptor::PrefetchType type,
                                                 const std::uint64_t size) noexcept
    {
        return PrefetchDescriptor::make_size(type, size);
    }
};

class PrefetchMask
{
public:
    constexpr PrefetchMask() noexcept = default;
    ~PrefetchMask() noexcept = default;

    void set(const std::uint8_t index) noexcept { _data |= (1U << index); }

    [[nodiscard]] PrefetchDescriptor make(const PrefetchDescriptor::PrefetchType type) const noexcept
    {
        return PrefetchDescriptor::make_mask(type, _data);
    }

    [[nodiscard]] static PrefetchDescriptor make(const PrefetchDescriptor::PrefetchType type,
                                                 PrefetchDescriptor::data_t data) noexcept
    {
        return PrefetchDescriptor::make_mask(type, data);
    }

private:
    PrefetchDescriptor::data_t _data{0U};
};

class PrefetchCallback
{
public:
    constexpr PrefetchCallback() noexcept = default;
    ~PrefetchCallback() noexcept = default;

    [[nodiscard]] static PrefetchDescriptor make(const std::uint8_t size, const std::uintptr_t callback) noexcept
    {
        const auto data =
            (PrefetchDescriptor::data_t(size) << PrefetchCallbackView::bits_for_pointer()) |
            (callback & PrefetchDescriptor::data_t(std::pow(2, PrefetchCallbackView::bits_for_pointer()) - 1));
        return PrefetchDescriptor::make_callback(data);
    }
};

class PrefetchHint
{
public:
    [[nodiscard]] static PrefetchHint make_size(const PrefetchDescriptor::PrefetchType type, const std::uint64_t size,
                                                const resource::ptr resource) noexcept
    {
        return PrefetchHint{PrefetchSize::make(type, size), resource};
    }

    [[nodiscard]] static PrefetchHint make_callback(const std::uint8_t size, const std::uintptr_t callback,
                                                    const resource::ptr resource) noexcept
    {
        return PrefetchHint{PrefetchCallback::make(size, callback), resource};
    }

    constexpr PrefetchHint() noexcept = default;
    constexpr PrefetchHint(const PrefetchDescriptor descriptor, const resource::ptr resource) noexcept
        : _descriptor(descriptor), _resource(resource)
    {
    }

    constexpr PrefetchHint(const PrefetchHint &) noexcept = default;
    constexpr PrefetchHint(PrefetchHint &&) noexcept = default;

    ~PrefetchHint() noexcept = default;

    PrefetchHint &operator=(const PrefetchHint &) = default;
    PrefetchHint &operator=(PrefetchHint &&) = default;

    [[nodiscard]] bool empty() const noexcept { return _descriptor.empty(); }
    [[nodiscard]] PrefetchDescriptor descriptor() const noexcept { return _descriptor; }
    [[nodiscard]] PrefetchMaskView as_mask() const noexcept
    {
        return PrefetchMaskView{_descriptor.data_without_descriptor_bits()};
    }

    [[nodiscard]] PrefetchSizeView as_size() const noexcept
    {
        return PrefetchSizeView{_descriptor.data_without_descriptor_bits()};
    }

    [[nodiscard]] resource::ptr resource() const noexcept { return _resource; }

    bool operator==(const PrefetchHint &other) const noexcept
    {
        return _resource == other._resource && _descriptor == other._descriptor;
    }

private:
    PrefetchDescriptor _descriptor;
    resource::ptr _resource;
};
} // namespace mx::tasking
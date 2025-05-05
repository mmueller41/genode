#pragma once

#include "resource_interface.h"
#include <cassert>
#include <cstdint>
#include <mx/memory/alignment_helper.h>
#include <mx/memory/tagged_ptr.h>
#include <mx/synchronization/synchronization.h>
#include <mx/util/random.h>
#include <new>

namespace mx::resource {
/**
 * Information of a resource, stored within
 * the pointer to the resource.
 */
class information
{
public:
    constexpr information() noexcept = default;
    explicit information(const std::uint16_t worker_id,
                         const synchronization::primitive synchronization_primitive) noexcept
        : _worker_id(worker_id), _synchronization_primitive(static_cast<std::uint16_t>(synchronization_primitive))
    {
    }

    ~information() = default;

    [[nodiscard]] std::uint16_t worker_id() const noexcept { return _worker_id; }
    [[nodiscard]] synchronization::primitive synchronization_primitive() const noexcept
    {
        return static_cast<synchronization::primitive>(_synchronization_primitive);
    }

    void worker_id(const std::uint16_t worker_id) noexcept { _worker_id = worker_id; }
    void synchronization_primitive(const synchronization::primitive primitive) noexcept
    {
        _synchronization_primitive = static_cast<std::uint16_t>(primitive);
    }

    information &operator=(const information &other) = default;

private:
    std::uint16_t _worker_id : 12 {0U};
    std::uint16_t _synchronization_primitive : 4 {0U};
} __attribute__((packed));

/**
 * Pointer to a resource, stores information about
 * that resource.
 */
class ptr final : public memory::tagged_ptr<void, information>
{
public:
    constexpr ptr() noexcept = default;
    constexpr ptr(const std::nullptr_t /*nullptr*/) noexcept : memory::tagged_ptr<void, information>(nullptr) {}
    constexpr explicit ptr(void *ptr_, const information info = {}) noexcept
        : memory::tagged_ptr<void, information>(ptr_, info)
    {
    }
    ~ptr() = default;

    ptr &operator=(const ptr &other) noexcept = default;
    ptr &operator=(std::nullptr_t) noexcept
    {
        reset(nullptr);
        return *this;
    }

    [[nodiscard]] std::uint16_t worker_id() const noexcept { return info().worker_id(); }
    [[nodiscard]] synchronization::primitive synchronization_primitive() const noexcept
    {
        return info().synchronization_primitive();
    }
} __attribute__((packed));

/**
 * Casts the internal pointer of the resource pointer
 * to a pointer typed by the given template parameter.
 *
 * @param resource Resource to cast.
 * @return Pointer to the requested type.
 */
template <typename S> static auto *ptr_cast(const ptr resource) noexcept
{
    return resource.template get<S>();
}

} // namespace mx::resource

namespace std {
template <> struct hash<mx::resource::ptr>
{
    std::size_t operator()(const mx::resource::ptr ptr) const noexcept
    {
        return std::hash<void *>().operator()(ptr.get());
    }
};
} // namespace std
#pragma once
#include <cstdint>
#include <mx/synchronization/synchronization.h>
#include <variant>

namespace mx::resource {
enum expected_access_frequency : std::uint8_t
{
    excessive = 0U,
    high = 1U,
    normal = 2U,
    unused = 3U,
};

enum expected_read_write_ratio : std::uint8_t
{
    heavy_read = 0U,
    mostly_read = 1U,
    balanced = 2U,
    mostly_written = 3U,
    heavy_written = 4U
};

class annotation
{
public:
    constexpr annotation() noexcept = default;
    constexpr explicit annotation(const std::uint8_t node_id) noexcept : _target(node_id) {}
    constexpr explicit annotation(const std::uint16_t worker_id) noexcept : _target(worker_id) {}
    constexpr explicit annotation(const synchronization::isolation_level isolation_level) noexcept
        : _isolation_level(isolation_level)
    {
    }
    constexpr explicit annotation(const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency)
    {
    }
    constexpr annotation(const std::uint16_t worker_id, const synchronization::isolation_level isolation_level) noexcept
        : _target(worker_id), _isolation_level(isolation_level)
    {
    }
    constexpr annotation(const std::uint8_t node_id, const synchronization::isolation_level isolation_level) noexcept
        : _target(node_id), _isolation_level(isolation_level)
    {
    }
    constexpr annotation(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                         const synchronization::protocol preferred_protocol) noexcept
        : _target(node_id), _isolation_level(isolation_level), _preferred_protocol(preferred_protocol)
    {
    }

    constexpr annotation(const std::uint16_t worker_id, const synchronization::isolation_level isolation_level,
                         const synchronization::protocol preferred_protocol) noexcept
        : _target(worker_id), _isolation_level(isolation_level), _preferred_protocol(preferred_protocol)
    {
    }

    constexpr annotation(const std::uint8_t node_id, const expected_access_frequency access_frequency) noexcept
        : _target(node_id), _access_frequency(access_frequency)
    {
    }
    constexpr annotation(const synchronization::isolation_level isolation_level,
                         const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency), _isolation_level(isolation_level)
    {
    }
    constexpr annotation(const synchronization::isolation_level isolation_level,
                         const synchronization::protocol preferred_protocol,
                         const expected_access_frequency access_frequency) noexcept
        : _access_frequency(access_frequency), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }
    constexpr annotation(const synchronization::isolation_level isolation_level,
                         const synchronization::protocol preferred_protocol,
                         const expected_access_frequency access_frequency,
                         const expected_read_write_ratio read_write_ratio) noexcept
        : _access_frequency(access_frequency), _read_write_ratio(read_write_ratio), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }
    constexpr annotation(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                         const expected_access_frequency access_frequency) noexcept
        : _target(node_id), _access_frequency(access_frequency), _isolation_level(isolation_level)
    {
    }
    constexpr annotation(const std::uint8_t node_id, const synchronization::isolation_level isolation_level,
                         const synchronization::protocol preferred_protocol,
                         const expected_access_frequency access_frequency) noexcept
        : _target(node_id), _access_frequency(access_frequency), _isolation_level(isolation_level),
          _preferred_protocol(preferred_protocol)
    {
    }

    constexpr annotation(annotation &&) noexcept = default;
    constexpr annotation(const annotation &) noexcept = default;

    ~annotation() = default;

    annotation &operator=(annotation &&) noexcept = default;
    annotation &operator=(const annotation &) noexcept = default;

    [[nodiscard]] bool has_numa_node_id() const noexcept { return std::holds_alternative<std::uint8_t>(_target); }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return std::get<std::uint8_t>(_target); }

    [[nodiscard]] bool has_worker_id() const noexcept { return std::holds_alternative<std::uint16_t>(_target); }
    [[nodiscard]] std::uint16_t worker_id() const noexcept { return std::get<std::uint16_t>(_target); }
    [[nodiscard]] expected_access_frequency access_frequency() const noexcept { return _access_frequency; }
    [[nodiscard]] expected_read_write_ratio read_write_ratio() const noexcept { return _read_write_ratio; }
    [[nodiscard]] synchronization::isolation_level isolation_level() const noexcept { return _isolation_level; }
    [[nodiscard]] synchronization::protocol preferred_protocol() const noexcept { return _preferred_protocol; }

    bool operator==(const synchronization::isolation_level isolation_level) const noexcept
    {
        return _isolation_level == isolation_level;
    }

    bool operator!=(const synchronization::isolation_level isolation_level) const noexcept
    {
        return _isolation_level != isolation_level;
    }

    bool operator==(const synchronization::protocol protocol) const noexcept { return _preferred_protocol == protocol; }

    bool operator!=(const synchronization::protocol protocol) const noexcept { return _preferred_protocol != protocol; }

private:
    // Preferred NUMA region or CPU core (if any).
    std::variant<std::uint8_t, std::uint16_t, std::monostate> _target{std::monostate{}};

    // Expected access frequency; normal by default.
    enum expected_access_frequency _access_frequency
    {
        expected_access_frequency::normal
    };

    // Expected read/write ratio; normal by default.
    expected_read_write_ratio _read_write_ratio{expected_read_write_ratio::balanced};

    // Preferred isolation level; no synchronization by default.
    synchronization::isolation_level _isolation_level{synchronization::isolation_level::None};

    // Preferred synchronization protocol (queue, latch, ...); no synchronization by default.
    synchronization::protocol _preferred_protocol{synchronization::protocol::None};
};
} // namespace mx::resource
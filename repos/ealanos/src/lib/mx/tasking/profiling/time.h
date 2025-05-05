#pragma once

#include <chrono>
#include <cstdint>
#include <ealanos/util/json.hpp>
#include <vector>

namespace mx::tasking::profiling {
class NormalizedTimeRange
{
public:
    constexpr NormalizedTimeRange(const std::chrono::nanoseconds start, const std::chrono::nanoseconds end) noexcept
        : _start(start), _end(end)
    {
    }

    NormalizedTimeRange(NormalizedTimeRange &&) noexcept = default;

    ~NormalizedTimeRange() noexcept = default;

    [[nodiscard]] std::chrono::nanoseconds start() const noexcept { return _start; }
    [[nodiscard]] std::chrono::nanoseconds end() const noexcept { return _end; }
    [[nodiscard]] std::chrono::nanoseconds duration() const noexcept { return _end - _start; }

private:
    std::chrono::nanoseconds _start;
    std::chrono::nanoseconds _end;
};

/**
 * Time range (from -- to) for idled time of a single channel.
 */
class TimeRange
{
public:
    TimeRange() noexcept : _start(std::chrono::system_clock::now()) {}
    constexpr explicit TimeRange(const std::chrono::system_clock::time_point start) noexcept : _start(start) {}
    constexpr TimeRange(const std::chrono::system_clock::time_point start,
                        const std::chrono::system_clock::time_point end) noexcept
        : _start(start), _end(end)
    {
    }
    constexpr TimeRange(TimeRange &&) noexcept = default;
    ~TimeRange() = default;

    /**
     * Sets the end of the idle range to the current time.
     */
    void stop() noexcept { _end = std::chrono::system_clock::now(); }

    /**
     * @return Number of nanoseconds idled.
     */
    [[nodiscard]] std::uint64_t nanoseconds() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(_end - _start).count();
    }

    /**
     * Normalizes this range with respect to a given point in time.
     * @param global_start Point in time to normalize.
     * @return Pair of (start, stop) normalized to the given time point.
     */
    [[nodiscard]] NormalizedTimeRange normalize(const std::chrono::system_clock::time_point global_start) const noexcept
    {
        return NormalizedTimeRange{std::max(_start, global_start) - global_start, _end - global_start};
    }

private:
    // Start of idling.
    std::chrono::system_clock::time_point _start;

    // End of idling.
    std::chrono::system_clock::time_point _end;
};

class WorkerIdleFrames
{
public:
    WorkerIdleFrames(std::vector<std::vector<std::chrono::nanoseconds>> &&idle_frames,
                     const std::chrono::nanoseconds duration, const std::chrono::nanoseconds frame_size) noexcept
        : _duration(duration), _frame_size(frame_size), _idle_frames(std::move(idle_frames))
    {
    }

    WorkerIdleFrames(WorkerIdleFrames &&) noexcept = default;

    ~WorkerIdleFrames() noexcept = default;

    [[nodiscard]] std::chrono::nanoseconds duration() const noexcept { return _duration; }
    [[nodiscard]] std::chrono::nanoseconds frame_size() const noexcept { return _frame_size; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return _idle_frames.size(); }
    [[nodiscard]] const std::vector<std::vector<std::chrono::nanoseconds>> &idle_frames() const noexcept
    {
        return _idle_frames;
    }
    [[nodiscard]] nlohmann::json to_json() const noexcept;

private:
    const std::chrono::nanoseconds _duration;
    const std::chrono::nanoseconds _frame_size;
    std::vector<std::vector<std::chrono::nanoseconds>> _idle_frames;
};

class IdleTimes
{
public:
    IdleTimes(std::vector<std::vector<NormalizedTimeRange>> &&idle_ranges,
              const std::chrono::nanoseconds duration) noexcept
        : _duration(duration), _idle_ranges(std::move(idle_ranges))
    {
    }

    IdleTimes(IdleTimes &&) noexcept = default;

    ~IdleTimes() noexcept = default;

    [[nodiscard]] std::chrono::nanoseconds duration() const noexcept { return _duration; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return _idle_ranges.size(); }
    [[nodiscard]] const std::vector<std::vector<NormalizedTimeRange>> &idle_ranges() const noexcept
    {
        return _idle_ranges;
    }

    [[nodiscard]] WorkerIdleFrames group(std::chrono::nanoseconds frame_size) const noexcept;

private:
    const std::chrono::nanoseconds _duration;
    std::vector<std::vector<NormalizedTimeRange>> _idle_ranges;
};
} // namespace mx::tasking::profiling
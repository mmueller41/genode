#include "time.h"

using namespace mx::tasking::profiling;

WorkerIdleFrames IdleTimes::group(const std::chrono::nanoseconds frame_size) const noexcept
{
    const auto count_frames = std::size_t(_duration / frame_size) + 1U;

    auto idle_frames = std::vector<std::vector<std::chrono::nanoseconds>>{};
    idle_frames.reserve(this->channels());

    for (auto worker_id = 0U; worker_id < this->channels(); ++worker_id)
    {
        auto frames = std::vector<std::chrono::nanoseconds>{count_frames, std::chrono::nanoseconds{0U}};

        for (const auto &time_range : this->_idle_ranges[worker_id])
        {
            const auto start_frame_id = time_range.start() / frame_size;
            const auto end_frame_id = time_range.end() / frame_size;
            if (start_frame_id == end_frame_id)
            {
                frames[start_frame_id] += time_range.duration();
            }
            else
            {
                /// Append to start.
                const auto start_frame_end = (start_frame_id + 1U) * frame_size;
                frames[start_frame_id] += start_frame_end - time_range.start();

                /// Append to end.
                const auto end_frame_start = end_frame_id * frame_size;
                frames[end_frame_id] += (time_range.end() - end_frame_start) + std::chrono::nanoseconds(1U);

                /// Fill frames between.
                for (auto frame_id = start_frame_id + 1U; frame_id <= end_frame_id - 1U; ++frame_id)
                {
                    frames[frame_id] += frame_size;
                }
            }
        }

        idle_frames.emplace_back(std::move(frames));
    }

    return WorkerIdleFrames{std::move(idle_frames), this->_duration, frame_size};
}

nlohmann::json WorkerIdleFrames::to_json() const noexcept
{
    nlohmann::json idle_times;
    idle_times["duration"] = this->_duration.count();
    idle_times["frame-size"] = this->_frame_size.count();
    idle_times["count-channels"] = this->channels();
    idle_times["count-frames"] = this->_idle_frames.front().size();

    nlohmann::json channels;
    for (const auto &channel_frames : this->_idle_frames)
    {
        nlohmann::json channel;
        for (const auto frame : channel_frames)
        {
            channel.emplace_back(frame.count());
        }
        channels.emplace_back(std::move(channel));
    }
    idle_times["channels"] = std::move(channels);

    return idle_times;
}
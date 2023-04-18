#pragma once
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <base/trace/perf.h>


/*
 * For more Performance Counter take a look into the Manual from Intel:
 *  https://software.intel.com/sites/default/files/managed/8b/6e/335279_performance_monitoring_events_guide.pdf
 *
 * To get event ids from manual specification see libpfm4:
 *  http://www.bnikolic.co.uk/blog/hpc-prof-events.html
 * Clone, Make, use examples/check_events to generate event id code from event:
 *  ./check_events <category>:<umask>[:c=<cmask>]
 * Example:
 *  ./cycle_activity:0x14:c=20
 */

namespace benchmark {

/**
 * Represents a Linux Performance Counter.
 */
class PerfCounter
{
public:
    PerfCounter(std::string &&name, const Genode::Trace::Performance_counter::Type type, const std::uint64_t event_id, const std::uint64_t mask) : _name(std::move(name)), _type(type), _event_id(static_cast<Genode::uint64_t>(event_id)), _mask(static_cast<Genode::uint64_t>(mask))
    {
        
    }

    ~PerfCounter() = default;

    bool open()
    {
        try {
            _counter = Genode::Trace::Performance_counter::acquire(_type);
        } catch (Genode::Trace::Pfc_no_avail) {
            std::cerr << "Failed to open performance counters." << std::endl;
        }

        try {
            Genode::Trace::Performance_counter::setup(_counter, _event_id, _mask, (_type == Genode::Trace::Performance_counter::Type::CORE ? 0x30000 : 0x550f000000000000));
        } catch (Genode::Trace::Pfc_access_error &e) {
            std::cerr << "Error while setting up performance counter: " << e.error_code() << std::endl;
        }

        return _counter >= 0;
    }

    bool start()
    {
        try {
            Genode::Trace::Performance_counter::start(_counter);
            _prev.value = static_cast<std::uint64_t>(Genode::Trace::Performance_counter::read(_counter));
        }
        catch (Genode::Trace::Pfc_access_error &e)
        {
            std::cerr << "Failed to start counter: " << e.error_code() << std::endl;
        }
        return _prev.value >= 0;
    }

    bool stop()
    {
        try {
            _data.value = Genode::Trace::Performance_counter::read(_counter);
            Genode::Trace::Performance_counter::stop(_counter);
            Genode::Trace::Performance_counter::reset(_counter);
        }
        catch (Genode::Trace::Pfc_access_error &e)
        {
            std::cerr << "Failed to stop counter: " << e.error_code() << std::endl;
        }
        // const auto is_read = ::read(_file_descriptor, &_data, sizeof(read_format)) == sizeof(read_format);
        // ioctl(_file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
        return _data.value >= 0; // is_read;
    }

    [[nodiscard]] double read() const
    {
        return static_cast<double>(_data.value - _prev.value);
    }

    [[nodiscard]] const std::string &name() const { return _name; }
    explicit operator const std::string &() const { return name(); }

    bool operator==(const std::string &name) const { return _name == name; }

private:
    struct read_format
    {
        std::uint64_t value = 0;
        std::uint64_t time_enabled = 0;
        std::uint64_t time_running = 0;
    };

    const std::string _name;
    Genode::Trace::Performance_counter::Type _type;
    Genode::uint64_t _event_id;
    Genode::uint64_t _mask;
    Genode::Trace::Performance_counter::Counter _counter;
    read_format _prev{};
    read_format _data{};
};

/**
 * Holds a set of performance counter and starts/stops them together.
 */
class Perf
{
public:
    [[maybe_unused]] static PerfCounter INSTRUCTIONS;
    [[maybe_unused]] static PerfCounter CYCLES;
    [[maybe_unused]] static PerfCounter L1_MISSES;
    [[maybe_unused]] [[maybe_unused]] static PerfCounter LLC_MISSES;
    [[maybe_unused]] static PerfCounter LLC_REFERENCES;
    //[[maybe_unused]] static PerfCounter STALLED_CYCLES_BACKEND;
    //[[maybe_unused]] static PerfCounter STALLS_MEM_ANY;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_NTA;
    //[[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_T0;
    //[[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_T1_T2;
    [[maybe_unused]] static PerfCounter SW_PREFETCH_ACCESS_WRITE;

    Perf() noexcept = default;
    ~Perf() noexcept = default;

    bool add(PerfCounter &counter_)
    {
        if (counter_.open())
        {
            _counter.push_back(counter_);
            return true;
        }

        return false;
    }

    void start()
    {
        for (auto &counter_ : _counter)
        {
            counter_.start();
        }
    }

    void stop()
    {
        for (auto &counter_ : _counter)
        {
            counter_.stop();
        }
    }

    double operator[](const std::string &name) const
    {
        auto counter_iterator = std::find(_counter.begin(), _counter.end(), name);
        if (counter_iterator != _counter.end())
        {
            return counter_iterator->read();
        }

        return 0.0;
    }

    std::vector<PerfCounter> &counter() { return _counter; }

private:
    std::vector<PerfCounter> _counter;
};
} // namespace benchmark
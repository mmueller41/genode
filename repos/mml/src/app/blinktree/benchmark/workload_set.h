#pragma once

#include "phase.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <libc/component.h>
namespace benchmark {
class NumericTuple
{
public:
    enum Type
    {
        INSERT,
        LOOKUP,
        UPDATE,
        DELETE
    };

    constexpr NumericTuple(const Type type, const std::uint64_t key) : _type(type), _key(key) {}
    constexpr NumericTuple(const Type type, const std::uint64_t key, const std::int64_t value)
        : _type(type), _key(key), _value(value)
    {
    }

    NumericTuple(NumericTuple &&) noexcept = default;
    NumericTuple(const NumericTuple &) = default;

    ~NumericTuple() = default;

    NumericTuple &operator=(NumericTuple &&) noexcept = default;

    [[nodiscard]] std::uint64_t key() const { return _key; };
    [[nodiscard]] std::int64_t value() const { return _value; }

    bool operator==(const Type type) const { return _type == type; }

private:
    Type _type;
    std::uint64_t _key;
    std::int64_t _value = 0;
};

class NumericWorkloadSet
{
    friend std::ostream &operator<<(std::ostream &stream, const NumericWorkloadSet &workload_set);
    friend class Fill_thread;
    friend class Mixed_thread;

public:
    NumericWorkloadSet(Libc::Env &env) : _env(env) {}
    ~NumericWorkloadSet() = default;
    Libc::Env &_env;

    void build(const std::string &fill_workload_file, const std::string &mixed_workload_file);
    void build(std::uint64_t fill_inserts, std::uint64_t mixed_inserts, std::uint64_t mixed_lookups,
               std::uint64_t mixed_updates, std::uint64_t mixed_deletes);
    void shuffle();

    [[nodiscard]] const std::vector<NumericTuple> &fill() const noexcept { return _data_sets[0]; }
    [[nodiscard]] const std::vector<NumericTuple> &mixed() const noexcept { return _data_sets[1]; }
    const std::vector<NumericTuple> &operator[](const phase phase) const noexcept
    {
        return _data_sets[static_cast<std::uint16_t>(phase)];
    }

    explicit operator bool() const { return fill().empty() == false || mixed().empty() == false; }

private:
    std::array<std::vector<NumericTuple>, 2> _data_sets;
    bool _mixed_phase_contains_update = false;

    static std::ostream &nice_print(std::ostream &stream, std::size_t number) noexcept;
};

class Fill_thread : public Genode::Thread
{
    private:
        //Genode::Mutex &_mutex;
        const std::string &_fill_workload_file;
        bool (*parse)(std::ifstream &, std::vector<NumericTuple> &);
        NumericWorkloadSet &_workload_set;

    public:
        Fill_thread(Libc::Env &env, Genode::Mutex &mutex, std::string fill_workload_name, bool (*parse)(std::ifstream&, std::vector<NumericTuple>&), NumericWorkloadSet &workload_set)
            : Genode::Thread(env, Name("btree::fill_thread"), 4*4096), _fill_workload_file(fill_workload_name), _workload_set(workload_set) 
        {
            this->parse = parse;
        }

        void entry() {
            std::ifstream fill_file(_fill_workload_file);
            if (fill_file.good()) {
                parse(fill_file, _workload_set._data_sets[static_cast<std::size_t>(phase::FILL)]);
            } else {
                //_mutex.acquire();
                std::cerr << "Could not open workload file '" << _fill_workload_file << "'." << std::endl;
                //_mutex.release();
            }
        }
};

class Mixed_thread : public Genode::Thread 
{
    private:
        const std::string &_mixed_workload_file;
        bool (*parse)(std::ifstream &, std::vector<NumericTuple> &);
        NumericWorkloadSet &_workload_set;

    public:
        Mixed_thread(Libc::Env &env, Genode::Mutex &mutex, std::string mixed_workload_name, bool (*parse)(std::ifstream&, std::vector<NumericTuple>&), NumericWorkloadSet &workload_set)
        : Genode::Thread(env, Name("btree::mixed_thread"), 4*4096),
        _mixed_workload_file(mixed_workload_name), _workload_set(workload_set)
        {
            this->parse = parse;
        }

        void entry()
        {
            std::ifstream mixed_file(_mixed_workload_file);
            if (mixed_file.good()) {
                _workload_set._mixed_phase_contains_update = parse(mixed_file, _workload_set._data_sets[static_cast<std::size_t>(phase::MIXED)]);
            } else {
                //_mutex.acquire();
                std::cerr << "Could not open workload file '" << _mixed_workload_file << "'." << std::endl;
                //_mutex.release();
            }
        }
};
} // namespace benchmark

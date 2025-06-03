#pragma once
#include <libc/component.h>
#include <base/ram_allocator.h>
#include <base/affinity.h>
#include <cstdint>
#include <base/attached_rom_dataspace.h>

namespace mx::util {
    class core_set;
}
namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
private:
    /**
     * @brief Pointer to application's environment
     * @details The application's environment grants access to core services of EalánOS, such as thread creation and memory allocation.
     */
    Libc::Env *_env;
	util::core_set *_coreset;
	static unsigned long tsc_freq_khz;

public:
    Environment() = default;

    /**
     * @brief Get Enviroment of current application
     * 
     * @return Libc::Env& environment
     */
    Libc::Env &getenv() { return *_env; }
    
    /**
     * @brief Set libc environment for MxTasking 
     * @details 
     * @param env pointer to libc enviroment object
     */
    void setenv(Libc::Env *env) { _env = env; }

    /**
     * @brief Set core set to be used by memory allocators and scheduler
     * @param core_set the core_set to use
    */
    static void set_cores(util::core_set *core_set);

    /**
     * @brief returns the core set representing the physical environment MxTasking is running in
    */
    static util::core_set &cores();

    /**
     * @brief Get the instance object
     * 
     * @return Environment& singleton object of Environment
     */
    static Environment &get_instance() 
    {
        static Environment env;
        return env;
    }
    
    /**
    * @brief Quick access to libc environment
    * 
    * @return Libc::Env& 
    */
    static Libc::Env &env() { return Environment::get_instance().getenv(); }

    static Libc::Env *envp() { return Environment::get_instance()._env; }

    /**
     * @brief Set the libc env object
     * 
     * @details This method **must** be called before creating the MxTasking runtime, because the environment
     *          is vital for the initialization of the MxTasking runtime enviroment.
     * @param env pointer to application's environment
     */
    static void set_env(Libc::Env *env) {
        Environment::get_instance().setenv(env);
    }

	static void get_platform_info()
	{
		try {
			Genode::Attached_rom_dataspace info(env(), "platform_info");
			tsc_freq_khz =
				info.xml().sub_node("hardware").sub_node("tsc").attribute_value("freq_khz", 0ULL);
		} catch (...) {
		};
	}

	static unsigned long get_cpu_freq()
	{
		return tsc_freq_khz;
	}

		/*
     * Access methods to core sessions of the libc enviroment, MxTasking is running in
     */
    static Genode::Cpu_session &cpu() { return Environment::env().cpu(); }
    static Genode::Ram_allocator &ram() { return Environment::env().ram(); }
    static Genode::Region_map &rm() { return Environment::env().rm(); }

    /**
     * @brief Convert integral core_id to location in the affinity space of the component
     * 
     * @param core_id The core_id to convert
     * @return Genode::Affinity::Location const& corresponding location in affinity space
     */
    static Genode::Affinity::Location location(std::uint16_t core_id) { return cpu().affinity_space().location_of_index(core_id); }
    

    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        return false;
    }

    static constexpr auto is_sse2()
    {
#ifdef USE_SSE2
        return true;
#else
        return false;
#endif
    }
};
} // namespace mx::system
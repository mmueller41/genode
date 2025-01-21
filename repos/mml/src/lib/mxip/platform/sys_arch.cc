/*
 * \brief  lwIP platform support
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2016-12-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <util/reconstructible.h>
#include <base/sleep.h>
#include <base/log.h>

#include <lwip/genode_init.h>

/* MxTasking includes */
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <mx/memory/dynamic_size_allocator.h>

extern "C" {
/* LwIP includes */
#include <lwip/timeouts.h>
#include <lwip/init.h>
#include <lwip/sys.h>

/* our abridged copy of string.h */
#include <cstring>
}

namespace Mxip {

	static mx::memory::dynamic::Allocator *_heap;
	class Timeout_task : public mx::tasking::TaskInterface
	{
	public:
		Timeout_task() {}

		mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t) override
		{
			//GENODE_LOG_TSC_NAMED(1, "sys_check_timeouts");
			sys_check_timeouts();
			_heap->free(static_cast<void *>(this));
			return mx::tasking::TaskResult::make_null();
		}
	};

	struct Mx_timer
	{
		void check_timeouts(Genode::Duration)
		{
			Timeout_task *task = new (_heap->allocate(0, 64, sizeof(Timeout_task))) Timeout_task(); // mx::tasking::runtime::new_task<Timeout_task>(0);
			if (task == nullptr) {
				Genode::error("Failed to allocate timeout task");
				return;
			}
			task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
			mx::tasking::runtime::spawn(*task);
		}

		::Timer::Connection &timer;

		Timer::Periodic_timeout<Mx_timer> timeout{
			timer, *this, &Mx_timer::check_timeouts, Genode::Microseconds{250 * 1000}};

		Mx_timer(::Timer::Connection &timer) : timer(timer) {}
	};

	static Mx_timer *sys_timer_ptr;

	void mxip_init(mx::memory::dynamic::Allocator &heap, ::Timer::Connection &timer)
	{
		_heap = &heap;

		static Mx_timer sys_timer(timer);
		sys_timer_ptr = &sys_timer;

		lwip_init();
	}

}


extern "C" {

	void lwip_platform_assert(char const* msg, char const *file, int line)
	{
		Genode::error("Assertion \"", msg, "\" ", file, ":", line);
		Genode::sleep_forever();
	}

	void genode_free(void *ptr)
	{
		Mxip::_heap->free(ptr);
	}

	void *genode_malloc(unsigned long size)
	{
		return Mxip::_heap->allocate(0, 64, size);
	}

	void *genode_calloc(unsigned long number, unsigned long size)
	{
		size *= number;

		void * const ptr = genode_malloc(size);
		if (ptr)
			Genode::memset(ptr, 0x00, size);

		return ptr;
	}

	u32_t sys_now(void) {
		/* TODO: Use actual CPU frequency */
		//return (u32_t)Mxip::sys_timer_ptr->timer.curr_time().trunc_to_plain_ms().value;
		return __builtin_ia32_rdtsc() / 2000000;
	}

	void genode_memcpy(void *dst, const void *src, size_t len)
	{
		std::memcpy(dst, src, len); }

	void *genode_memmove(void *dst, const void *src, size_t len) {
		return std::memmove(dst, src, len); }

	int memcmp(const void *b1, const void *b2, ::size_t len) {
		return std::memcmp(b1, b2, len); }

	int strcmp(const char *s1, const char *s2)
	{
		size_t len = std::min(Genode::strlen(s1), Genode::strlen(s2));
		return  std::strncmp(s1, s2, len);
	}

	int strncmp(const char *s1, const char *s2, size_t len) {
		return std::strncmp(s1, s2, len); }

}

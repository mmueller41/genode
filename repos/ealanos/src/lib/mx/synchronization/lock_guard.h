#pragma once

namespace mx::synchronization {
	template <class LOCK> class Lock_guard
	{
		private:

			LOCK &_lock;

		public:

			Lock_guard(LOCK lock) : _lock(lock) { _lock.lock(); }
			~Lock_guard() { _lock.unlock(); }
	};
}

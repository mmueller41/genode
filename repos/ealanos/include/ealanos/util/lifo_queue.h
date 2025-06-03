#ifndef __INCLUDE__EALANOS__UTIL_LIFO_QUEUE_H_
#define __INCLUDE__EALANOS__UTIL_LIFO_QUEUE_H_

namespace Ealan::util
{
	template <class T>
    class Lifo_queue;
}

template <class T>
class Ealan::util::Lifo_queue
{
	private:

		alignas(64) T *_head{nullptr};

	public:

		void enqueue(T *elem)
		{
			T *next = __atomic_exchange_n(&_head, elem, __ATOMIC_RELAXED);
			elem->next(next);
		}

		T *dequeue()
		{
			if (_head) {
			    T *elem = __atomic_exchange_n(&_head, _head->next(), __ATOMIC_RELAXED);
				return elem;
			}
			return nullptr;
		}

		T *head()
		{
			return _head;
		}
};
#endif 
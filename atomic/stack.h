#ifndef __COMM_ATOMIC_STACK_H__
#define __COMM_ATOMIC_STACK_H__

#include "atomic.h"

namespace atomic {

template<class T>
struct stack_node
{
	stack_node() : _nexts(0) {}

	T * volatile _nexts;
};

template <class T>
struct DefaultSelector
{
	static stack_node<T>* get(T * const p) { return static_cast<stack_node<T>*>(p); }
};

template <class T, class S = DefaultSelector<T> >
class stack
{
public:
	typedef stack_node<T> node_t;

private:
	struct ptr_t
	{
		ptr_t() : _ptr(0), _pops(0) {}

		ptr_t(T * const ptr, const uint pops)
			: _ptr(ptr), _pops(pops) {}

		union {
			struct {
		        T * volatile _ptr;
			    volatile uint _pops;
			};
			volatile int64 _data;
		};
	};

	ptr_t _head;

public:
	stack()	: _head() {}

	void push(T * item)
	{
		for (;;) {
			S::get(item)->_nexts = _head._ptr;
			if (b_cas_ptr(
			    reinterpret_cast<void*volatile*>(&_head._ptr),
			    item, 
			    S::get(item)->_nexts))
				break;
		}
	}

	T * pop()
	{
		for (;;) {
			const ptr_t head = _head;

			if (head._ptr == 0) return 0;

			const ptr_t next(S::get(head._ptr)->_nexts, head._pops + 1);

			if (b_cas(&_head._data, next._data, head._data)) {
				return static_cast<T*>(head._ptr);
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_STACK_H__
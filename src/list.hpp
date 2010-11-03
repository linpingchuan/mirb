#pragma once
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

namespace Mirb
{
	template<class T> class Entry
	{
	public:
		Entry() : next(0), prev(0) {}

		T *next;
		T* prev;
	};

	template<class T, class E = T, Entry<E> E::*field = &E::entry> class List
	{
	public:
		List() : first(0), last(0) {}
		
		T *first;
		T *last;
		
		bool empty()
		{
			return first == 0;
		}
		
		void remove(T *node)
		{
			assert(node);

			Entry<E> &entry = node->*field;
			
			if(entry.prev)
				(entry.prev->*field).next = entry.next;
			else
				first = static_cast<T *>(entry.next);

			if(entry.next)
				(entry.next->*field).prev = entry.prev;
			else
				last = static_cast<T *>(entry.prev);
		}

		void append(T *node)
		{
			assert(node);

			Entry<E> &entry = node->*field;

			entry.next = 0;

			if(last)
			{
				entry.prev = static_cast<E *>(last);
				(last->*field).next = static_cast<E *>(node);
				last = node;
			}
			else
			{
				first = node;
				last = node;
			}
		}

		class Iterator
		{
		private:
			T *current;

		public:
			Iterator(List &list) : current(list.first) {}

			void step()
			{
				Entry<E> &entry = current->*field;
				current = static_cast<T *>(entry.next);
			}

			operator bool()
			{
				return current != 0;
			}

			T &operator ++()
			{
				step();
				return *current;
			}

			T &operator ++(int)
			{
				T *result = current;
				step();
				return *result;
			}

			T *operator*()
			{
				return current;
			}

			T &operator ()()
			{
				return *current;
			}
		};
		
		Iterator begin()
		{
			return Iterator(*this);
		}
	};
};

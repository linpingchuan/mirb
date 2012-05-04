#pragma once
#include "class.hpp"
#include "../context.hpp"

namespace Mirb
{
	class Array:
		public Object
	{
		private:
			static value_t allocate(value_t obj);
			static value_t push(value_t obj, size_t argc, value_t argv[]);
			static value_t pop(value_t obj);
			static value_t unshift(value_t obj, size_t argc, value_t argv[]);
			static value_t inspect(value_t obj);
			static value_t length(value_t obj);
			static value_t each(value_t obj, value_t block);

		public:
			Array(Class *instance_of) : Object(Value::Array, instance_of) {}
			Array() : Object(Value::Array, context->array_class) {}

			Vector<value_t, AllocatorBase, Allocator> vector;
			
			template<typename F> void mark(F mark)
			{
				Object::mark(mark);

				vector.mark(mark);
			}

			static void initialize();
	};
};

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
			static value_t get(value_t obj, size_t index);
			static value_t set(value_t obj, size_t index, value_t value);

		public:
			Array(Class *instance_of) : Object(Value::Array, instance_of) {}
			Array() : Object(Value::Array, context->array_class) {}

			Vector<value_t, AllocatorBase, Allocator> vector;
			
			template<typename F> void mark(F mark)
			{
				Object::mark(mark);

				vector.mark(mark);
			}

			template<typename F> static void parse(const char_t *input, size_t length, F func)
			{
				const char_t *end = input + length;

				auto is_white = [&] {
					return (*input >= 1 && *input <= 32);
				};

				auto skip_white = [&] {
					while(is_white() && input < end)
						input++;
				};
				
				auto push = [&] {
					const char_t *start = input;

					while(input < end && !is_white())
						input++;

					std::string out((const char *)start, (size_t)input - (size_t)start);

					func(out);
				};

				while(input < end)
				{
					skip_white();

					if(input < end)
						push();
				}
			}
			
			static void initialize();
	};
};

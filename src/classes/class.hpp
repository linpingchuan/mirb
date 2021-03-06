#pragma once
#include "../value.hpp"
#include "../block.hpp"
#include "../context.hpp"
#include "module.hpp"

namespace Mirb
{
	class Class:
		public Module
	{
		private:
			static value_t to_s(Class *self);
			static value_t rb_superclass(Module *obj);
			static value_t rb_new(value_t obj, size_t argc, value_t argv[], value_t block);
			static value_t case_equal(Class *obj, value_t other);
			static value_t rb_allocate(Class *instance_of);

			Class(Type::Enum type) : Module(type), singleton(false) {}

		public:
			Class(Type::Enum type, Class *instance_of, Class *superclass, bool singleton = false) : Module(type, instance_of, superclass), singleton(singleton) {}
			Class(Class *instance_of) : Module(Type::Class, instance_of, context->object_class), singleton(false) {}

			bool singleton;
			
			static Class *create_initial(Class *superclass);
			static Class *create_include_class(Module *module, Class *superclass);

			template<typename F> void mark(F mark)
			{
				Module::mark(mark);
			}

			static void initialize();
	};
};


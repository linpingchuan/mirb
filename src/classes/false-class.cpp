#include "false-class.hpp"
#include "object.hpp"
#include "symbol.hpp"
#include "string.hpp"
#include "../runtime.hpp"

namespace Mirb
{
	value_t FalseClass::to_s()
	{
		return String::get("false");
	}

	void FalseClass::initialize()
	{
		method(context->false_class, "to_s", &to_s);

		set_const(context->object_class, Symbol::get("FALSE"), value_false);
	};
};


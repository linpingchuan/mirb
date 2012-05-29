#include "platform.hpp"
#include "../runtime.hpp"

namespace Mirb
{
	namespace Platform
	{
		CharArray native_path(const CharArray &path)
		{
			if(!path.size())
				raise(context->system_call_error, "Empty path");

			return File::expand_path(path);
		}

		CharArray ruby_path(const CharArray &path)
		{
			return path;
		}
	};
};

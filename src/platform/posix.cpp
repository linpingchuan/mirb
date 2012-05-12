#include "platform.hpp"

#ifndef WIN32

#include "../collector.hpp"
#include <signal.h>
#include <unistd.h>

namespace Mirb
{
	namespace Platform
	{
		std::string BenchmarkResult::format()
		{
			std::stringstream result;

			result << ((double)time / 1000) << " ms";

			return result.str();
		}

		void *allocate_region(size_t bytes)
		{
			void *result = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			mirb_runtime_assert(result != 0);

			return result;
		}
		
		void free_region(void *region, size_t bytes)
		{
			munmap(region, bytes);
		}
		
		CharArray cwd()
		{
			char *result = getcwd(nullptr, 0);

			CharArray str((const char_t *)result, std::strlen(result));

			std::free(result);

			return str;
		}
		
		bool file_exists(const CharArray &file)
		{
			CharArray file_cstr = file.c_str();

			struct stat buf;

			if(stat(file_cstr.c_str_ref(), &buf) == 0)
				return true;
			else
				return false;
		}

		void signal_handler(int)
		{
			Collector::signal();
		}

		void initialize()
		{
			signal(SIGINT, signal_handler);
		}
	};
};

#endif

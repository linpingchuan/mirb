#pragma once

#include <cstdbool>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cassert>

#define RT_ALIGN(value, to) ((value + (to) - 1) & ~((to) - 1))

#ifdef DEBUG
	#define RT_ASSERT(condition) do { \
		if(!(condition)) { \
			__asm__("int3\n"); \
			assert(0); \
		} \
	} while(0)
#else
	#define RT_ASSERT(condition) assert(condition)
#endif

#define __regparm(n) __attribute__((__regparm__(n)))

#ifdef WIN32
    #include <windows.h>
    #include <excpt.h>
    #include "vendor/BeaEngine/BeaEngine.h"
#else
	#define __stdcall __attribute__((__stdcall__))
	#define __cdecl __attribute__((__cdecl__))

	#include <sys/mman.h>
	#include <sys/stat.h>
#endif

static inline void __attribute__((noreturn)) __builtin_unreachable(void)
{
	while(1);
}

#include "vector.hpp"
#include "hash.hpp"
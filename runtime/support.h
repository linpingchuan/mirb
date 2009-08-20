#pragma once
#include "runtime.h"

typedef struct {
	union {
		rt_value *upval;
		rt_value local;
	} val;
	bool sealed;
} rt_upval_t;

typedef rt_value (__cdecl *rt_compiled_closure_t)(rt_upval_t **upvals, rt_value obj, unsigned int argc, ...);

void __stdcall rt_support_seal_upval(rt_upval_t *upval);

rt_value rt_support_interpolate(unsigned int argc, ...);

rt_value rt_support_closure(rt_compiled_closure_t block, unsigned int argc, ...);

rt_value __stdcall rt_support_get_const(rt_value obj, rt_value name);
void __stdcall rt_support_set_const(rt_value obj, rt_value name, rt_value value);

rt_value __stdcall rt_support_define_string(const char* string);

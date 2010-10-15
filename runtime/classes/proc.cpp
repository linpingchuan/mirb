#include "../classes.hpp"
#include "../runtime.hpp"
#include "proc.hpp"
#include "symbol.hpp"
#include "string.hpp"

rt_value rt_Proc;

rt_compiled_block(rt_proc_call)
{
	return rt_call_proc(obj, block, argc, argv);
}

void rt_proc_init(void)
{
	rt_Proc = rt_define_class(rt_Object, "Proc", rt_Object);
	rt_define_method(rt_Proc, "call", (rt_compiled_block_t)rt_proc_call);
}
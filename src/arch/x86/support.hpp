#pragma once
#include "../../common.hpp"
#include "../../block.hpp"
#include "../../char-array.hpp"

namespace Mirb
{
	namespace Arch
	{
		namespace Support
		{
			struct Frame;
			
			extern Frame *current_frame;
			
			struct Frame
			{
				enum Type
				{
					NativeEntry,
					UnnamedNativeEntry,
					Exception
				};

				Frame(size_t type) : prev(current_frame), type(type)
				{
					current_frame = this;
				}

				~Frame()
				{
					current_frame = prev;
				}

				Frame *prev;
				size_t type;
			};

			struct FramePrefix
			{
				value_t module;
				size_t prev_bp; // bp = &prev_bp
				void *return_address;
				value_t object;
				Symbol *name;
				size_t argc;
				value_t *argv;

				CharArray inspect() const;

				static FramePrefix *from_bp(size_t bp)
				{
					return (FramePrefix *)(bp - sizeof(size_t));
				};

				const FramePrefix *prev() const
				{
					if(prev_bp)
						return from_bp(prev_bp);
					else
						return 0;
				};
			};

			struct ExceptionFrame:
				public Frame
			{
				size_t handling;
				size_t block_index;
				Mirb::Block *block;
				FramePrefix prefix;
			};
			
			struct NativeEntry:
				public Frame
			{
				NativeEntry() : Frame(Frame::NativeEntry) {}
				FramePrefix prefix;
			};

			struct UnnamedNativeEntry:
				public Frame
			{
				UnnamedNativeEntry(size_t bp) : Frame(Frame::UnnamedNativeEntry), bp(bp) {}
				size_t bp;
			};

			CharArray backtrace();

			void handle_exception(ExceptionFrame *frame, Exception *exception);
			
			void __noreturn exception_raise(Exception *exception);

			void __noreturn __fastcall far_return(size_t bp, size_t dummy, value_t value, Block *target);
			void __noreturn __fastcall far_break(size_t bp, size_t dummy, value_t value, Block *target, size_t id);
			
			void __noreturn raise();

			#ifdef _MSC_VER
				void jit_stub();
			#else
				extern void jit_stub() mirb_external("mirb_arch_support_jit_stub");
			#endif
			
			value_t closure_call(compiled_block_t code, value_t *scopes[], value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[]);
			
			value_t ruby_call(compiled_block_t code, value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[]);
			
			value_t *__stdcall create_heap(size_t bytes);
			value_t __cdecl create_closure(Block *block, value_t self, Symbol *name, value_t module, size_t argc, value_t *argv[]);
			value_t __cdecl create_array(size_t argc, value_t argv[]);
			value_t __cdecl interpolate(size_t argc, value_t argv[]);
			
			value_t __stdcall get_const(value_t obj, Symbol *name);
			void __stdcall set_const(value_t obj, Symbol *name, value_t value);
			
			value_t __stdcall get_ivar(value_t obj, Symbol *name);
			void __stdcall set_ivar(value_t obj, Symbol *name, value_t value);

			value_t __stdcall define_class(value_t obj, Symbol *name, value_t super);
			value_t __stdcall define_module(value_t obj, Symbol *name);
			void __stdcall define_method(value_t obj, Symbol *name, Block *block);
			value_t __stdcall define_string(const char *string);
			
			compiled_block_t __stdcall lookup(value_t obj, Symbol *name, value_t *result_module) mirb_external("mirb_arch_support_lookup");
			compiled_block_t __stdcall lookup_super(value_t module, Symbol *name, value_t *result_module) mirb_external("mirb_arch_support_lookup_super");

			value_t __fastcall call(value_t block, value_t dummy, value_t obj, Symbol *name, size_t argc, value_t argv[]) mirb_external("mirb_arch_support_call");
			value_t __fastcall super(value_t block, value_t module, value_t obj, Symbol *name, size_t argc, value_t argv[]) mirb_external("mirb_arch_support_super");
		};
	};
};

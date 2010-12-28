#include "support.hpp"
#include "../../support.hpp"
#include "../../compiler.hpp"
#include "../../runtime.hpp"
#include "../../classes/proc.hpp"
#include "../../classes/module.hpp"
#include "../../classes/string.hpp"
#include "../../classes/exceptions.hpp"

namespace Mirb
{
	namespace Arch
	{
		namespace Support
		{
			Frame *current_frame;

			CharArray FramePrefix::inspect() const
			{
				CharArray result;
				
				OnStack<1> os(result);
				
				value_t module = auto_cast(this->module);

				if(Value::type(module) == Value::IClass)
					module = cast<Module>(module)->instance_of;
				
				result += inspect_obj(module);

				value_t class_of = real_class_of(object);
				
				if(class_of != module)
					result += "(" + inspect_obj(class_of) + ")";

				result += "#" + name->string + " at <unknown>";

				return result;
			}

			CharArray backtrace()
			{
				Frame *current = current_frame;

				CharArray result;

				OnStack<1> os(result);

				auto list_block = [&](const FramePrefix *block)
				{
					if(result.size() != 0)
						result += "\n";
					
					result += block->inspect();
				};
				
				while(current)
				{
					const FramePrefix *frame;

					mirb_debug_assert(current->type == Frame::NativeEntry || current->type == Frame::UnnamedNativeEntry);

					if(current->type == Frame::NativeEntry)
					{
						NativeEntry *native_entry = (NativeEntry *)current;

						frame = &native_entry->prefix;
					}
					else
					{
						UnnamedNativeEntry *native_entry = (UnnamedNativeEntry *)current;
						frame = FramePrefix::from_bp(native_entry->bp);
					}

					mirb_debug_assert(frame != 0);

					do
					{
						list_block(frame);

						frame = frame->prev();
					}
					while(frame);

					do
					{
						current = current->prev;
					}
					while(current && (current->type != Frame::NativeEntry) && (current->type != Frame::UnnamedNativeEntry));
				}
				
				return result;
			}

			value_t *__stdcall create_heap(size_t bytes)
			{
				return Mirb::Support::create_heap(bytes);
			}

			value_t __stdcall define_class(value_t obj, Symbol *name, value_t super)
			{
				return Mirb::Support::define_class(obj, name, super);
			}
			
			value_t __stdcall define_module(value_t obj, Symbol *name)
			{
				return Mirb::Support::define_module(obj, name);
			}
			
			void __stdcall define_method(value_t obj, Symbol *name, Block *block)
			{
				return Mirb::Support::define_method(obj, name, block);
			}
			
			value_t __stdcall define_string(const char *string)
			{
				return Mirb::Support::define_string(string);
			}
			
			value_t __stdcall interpolate(size_t bp, size_t argc, value_t argv[])
			{
				UnnamedNativeEntry entry(bp); // We're entering native code without a name

				return Mirb::Support::interpolate(argc, argv);
			}
			
			value_t __stdcall get_const(size_t bp, value_t obj, Symbol *name)
			{
				value_t result = Mirb::Support::get_const(obj, name);

				if(mirb_unlikely(result == value_undef))
				{
					UnnamedNativeEntry entry(bp); // We're entering native code without a name

					Exception *exception = new Exception(NameError::class_ref, ("Uninitialized constant " + name->get_string() + " on " + pretty_inspect(obj)).to_string(), backtrace().to_string());

					exception_raise(exception);
				}

				return result;
			}

			void __stdcall set_const(size_t bp, value_t obj, Symbol *name, value_t value)
			{
				bool existing = Mirb::Support::set_const(obj, name, value);

				if(mirb_unlikely(existing))
				{
					UnnamedNativeEntry entry(bp); // We're entering native code without a name

					OnStack<1> os(value);
					std::cout << "Warning: Reassigning constant " << name->get_string() << " to " << inspect_object(value) << "\n";
				}
			}
			
			value_t __stdcall get_ivar(value_t obj, Symbol *name)
			{
				return Mirb::Support::get_ivar(obj, name);
			}

			void __stdcall set_ivar(value_t obj, Symbol *name, value_t value)
			{
				return Mirb::Support::set_ivar(obj, name, value);
			}

			value_t __stdcall create_array(size_t argc, value_t argv[])
			{
				return Mirb::Support::create_array(argc, argv);
			}

			void __noreturn raise() // Used for code within NativeEntry
			{
				exception_raise(current_exception);
			}
			
			Block::compiled_t __fastcall jit_invoke(Block *block) mirb_external("mirb_arch_support_jit_invoke");

			Block::compiled_t __fastcall jit_invoke(Block *block)
			{
				return Compiler::defered_compile(block);
			}

			void __fastcall unknown_call_method(size_t bp, value_t argv[], value_t obj, Symbol *name, value_t module, size_t block) mirb_external("mirb_arch_support_unknown_call_method");
			void __fastcall unknown_super_method(size_t bp, value_t argv[], value_t obj, Symbol *name, value_t module, size_t block) mirb_external("mirb_arch_support_unknown_super_method");
			
			void __fastcall unknown_call_method(size_t bp, value_t argv[], value_t obj, Symbol *name, value_t module, size_t block)
			{
				UnnamedNativeEntry entry(bp); // We're entering native code without a name

				Exception *exception = new Exception(NameError::class_ref, ("Undefined method '" + name->get_string() + "' for " + pretty_inspect(obj)).to_string(), backtrace().to_string());

				exception_raise(exception);
			}

			void __fastcall unknown_super_method(size_t bp, value_t argv[], value_t obj, Symbol *name, value_t module, size_t block)
			{
				UnnamedNativeEntry entry(bp); // We're entering native code without a name

				Exception *exception = new Exception(NameError::class_ref, ("No superclass method '" + name->get_string() + "' for " + pretty_inspect(module)).to_string(), backtrace().to_string());
				
				exception_raise(exception);
			}

			#ifdef _MSC_VER
				__declspec(naked) value_t __fastcall call(value_t block, value_t dummy, value_t obj, Symbol *name, size_t argc, value_t argv[])
				{
					__asm
					{
						lea eax, dword ptr [esp + 12]
						push eax
						push dword ptr [esp + 12]
						push dword ptr [esp + 12]
						call lookup
						test eax, eax
						jz unknown_method
						jmp eax

					unknown_method:
						mov ecx, ebp
						jmp unknown_call_method
					}
				}

				__declspec(naked) value_t __fastcall super(value_t block, value_t module, value_t obj, Symbol *name, size_t argc, value_t argv[])
				{
					__asm
					{
						lea eax, dword ptr [esp + 12]
						push eax
						push dword ptr [esp + 12]
						call lookup_super
						test eax, eax
						jz unknown_method
						jmp eax

					unknown_method:
						mov ecx, ebp
						jmp unknown_super_method
					}
				}
			
				value_t ruby_call(Block::compiled_t code, value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;

					__asm
					{
						push ebp
						push esi
						push edi
						push ebx
						push block
						push module
						push name
						push obj
						mov esi, argc
						mov edi, argv
						mov eax, code
						xor ebp, ebp
						call eax
						pop ebx
						pop edi
						pop esi
						pop ebp

						mov result, eax
					}

					return result;
				}

				value_t closure_call(Block::compiled_t code, value_t *scopes[], value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;

					__asm
					{
						push ebp
						push esi
						push edi
						push ebx
						push block
						push module
						push name
						push obj
						mov ebx, scopes
						mov esi, argc
						mov edi, argv
						mov eax, code
						xor ebp, ebp
						call eax
						pop ebx
						pop edi
						pop esi
						pop ebp

						mov result, eax
					}

					return result;
				}

				__declspec(naked) void jit_stub()
				{
					__asm
					{
						call jit_invoke
						jmp eax
					}
				}
			#else
				value_t ruby_call(Block::compiled_t code, value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;
					
					asm ("pushl %%ebp\n"
						"pushl %[block]\n"
						"pushl %[module]\n"
						"pushl %[name]\n"
						"pushl %[obj]\n"
						"xorl %%ebp, %%ebp\n"
						"call *%[code]\n"
						"popl %%ebp\n"
					: "=a"(result)
					: [block] "g"(block), [module] "g"(module), [name] "g"(name), [obj] "g"(obj), [code] "b"(code), "S"(argc), "D"(argv)
					: "ecx", "edx", "memory");
					
					return result;
				}

				value_t closure_call(Block::compiled_t code, value_t *scopes[], value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;
					
					asm ("pushl %%ebp\n"
						"pushl %[block]\n"
						"pushl %[module]\n"
						"pushl %[name]\n"
						"pushl %[obj]\n"
						"xorl %%ebp, %%ebp\n"
						"call *%[code]\n"
						"popl %%ebp\n"
					: "=a"(result)
					: [block] "g"(block), [module] "g"(module), [name] "g"(name), [obj] "g"(obj), [code] "a"(code), "b"(scopes), "S"(argc), "D"(argv)
					: "ecx", "edx", "memory");
					
					return result;
				}
			#endif
			
			value_t __stdcall create_closure(Block *block, value_t self, Symbol *name, value_t module, size_t argc, value_t *argv[])
			{
				return Mirb::Support::create_closure(block, self, name, module, argc, argv);
			}
			
			Block::compiled_t __stdcall lookup(value_t obj, Symbol *name, value_t *result_module)
			{
				return Mirb::lookup_nothrow(obj, name, result_module);
			}

			Block::compiled_t __stdcall lookup_super(Symbol *name, value_t *module)
			{
				return Mirb::lookup_super_nothrow(*module, name, module);
			}
			
			void __noreturn exception_raise(Exception *exception)
			{
				size_t bp_target;

				Frame *frame = current_frame;

				mirb_debug_assert(frame->type == Frame::UnnamedNativeEntry || frame->type == Frame::NativeEntry);
					
				if(frame->type == Frame::UnnamedNativeEntry)
					bp_target = ((UnnamedNativeEntry *)frame)->bp;
				else
					bp_target = (size_t)&((NativeEntry *)frame)->prefix.prev_bp;
				
				frame = frame->prev;

				while(frame && frame->type == Frame::Exception)
				{
					ExceptionFrame *exception_frame = (ExceptionFrame *)frame;
					handle_exception(exception_frame, exception);
				
					bp_target = (size_t)&exception_frame->prefix.prev_bp;

					frame = frame->prev;
				}
				
				current_frame = frame ? frame->prev : 0;
				current_exception = exception;
				
				#ifdef _MSC_VER
					__asm
					{
						mov esp, bp_target
						xor eax, eax
						pop ebp
						ret 16
					}
				#else
					__asm__("mov %0, %%esp\n"
						"xor %%eax, %%eax\n"
						"pop %%ebp\n"
						"ret $16\n"
					:
					: "r" (bp_target));
				#endif

				__builtin_unreachable();
			}

			void __noreturn __fastcall far_return(size_t bp, size_t dummy, value_t value, Block *target)
			{
				UnnamedNativeEntry entry(bp);
				
				Exception *exception = new ReturnException(Value::ReturnException, LocalJumpError::class_ref, String::from_literal("Unhandled return from block"), backtrace().to_string(), target, value);
				
				exception_raise(exception);
			}

			void __noreturn __fastcall far_break(size_t bp, size_t dummy, value_t value, Block *target, size_t id)
			{
				UnnamedNativeEntry entry(bp);
				
				Exception *exception = new BreakException(LocalJumpError::class_ref, String::from_literal("Unhandled break from block"), backtrace().to_string(), target, value, id);
				
				exception_raise(exception);
			}

			static void local_unwind(ExceptionFrame *frame, ExceptionBlock *block, ExceptionBlock *target)
			{
				frame->handling = 1;

				size_t ebp_value = (size_t)&frame->prefix.prev_bp;
				
				while(block != target)
				{
					void *target_label = block->ensure_label.address;

					frame->block_index = block->parent_index;

					if(target_label)
					{
						#ifdef DEBUG
							printf("Ensure block found\n");
							printf("Ebp: %x\n", ebp_value);
							printf("Eip: %x\n", (size_t)target_label);
						#endif

						// TODO: We need to mark this as a Ruby entry

						#ifdef _MSC_VER
							__asm
							{
								push ebx
								push esi
								push edi
								push ebp
								push ecx
								push edx

								mov eax, target_label
								mov ebp, ebp_value
								call eax
								
								pop edx
								pop ecx
								pop ebp
								pop edi
								pop esi
								pop ebx
							}
						#else
							__asm__ volatile("pushl %%ebp\n"
								"pushl %%ebx\n"
								"mov %%eax, %%ebp\n"
								"call *%%ecx\n"
								"popl %%ebx\n"
								"popl %%ebp\n"
							:
							: "a" (ebp_value), "c" (target_label)
							: "edx", "esi", "edi", "memory");
						#endif
					}

					block = block->parent;
				}

				frame->handling = 0;
			}
			
			static void __noreturn jump_to_handler(ExceptionFrame *frame, void *target, value_t value)
			{
				current_frame = frame;

				size_t ebp_value = (size_t)&frame->prefix.prev_bp;
				size_t esp_value = ebp_value - frame->block->ebp_offset;

				#ifdef DEBUG
					printf("Jump target found\n");
					printf("Ebp: %x\n", ebp_value);
					printf("Esp: %x\n", esp_value);
					printf("Eip: %x\n", (size_t)target);
				#endif
				
				#ifdef _MSC_VER
					__asm
					{
						mov eax, value
						mov ebx, ebp_value
						mov ecx, esp_value
						mov edx, target
						
						mov ebp, ebx
						mov esp, ecx
						jmp edx
					}
				#else
					__asm__ __volatile__("mov %0, %%ecx\n"
						"mov %1, %%esp\n"
						"mov %2, %%ebp\n"
						"jmp *%%ecx\n"
					:
					: "r" (target),
						"r" (esp_value),
						"r" (ebp_value),
						"a" (value)
					: "%ecx");
				#endif
				__builtin_unreachable();
			}

			static void __noreturn handle_return(ExceptionFrame *frame, ExceptionBlock *block, value_t value)
			{
				/*
				 * Execute frame local ensure handlers
				 */

				local_unwind(frame, block, 0);
				
				/*
				 * Set to the current handler index to the parent
				 */

				frame->block_index = block->parent_index;
				
				/*
				 * Go to the handler and never return
				 */

				jump_to_handler(frame, frame->block->epilog, value);
			}

			static void __noreturn handle_break(ExceptionFrame *frame, value_t value, size_t id)
			{
				/*
				 * Go to the handler and never return
				 */

				jump_to_handler(frame, frame->block->break_targets[id], value);
			}

			static void __noreturn rescue(ExceptionFrame *frame, ExceptionBlock *block, ExceptionBlock *current_block, void *rescue_label)
			{
				current_frame = frame;
				
				/*
				 * Execute frame local ensure handlers
				 */

				local_unwind(frame, block, current_block);

				/*
				 * Set to the current handler index to the parent
				 */

				frame->block_index = current_block->parent_index;

				/*
				 * Go to the handler and never return
				 */

				jump_to_handler(frame, rescue_label, 0);
			}
			
			void handle_exception(ExceptionFrame *frame, Exception *exception)
			{
				if(frame->block_index == (size_t)-1) // Outside any exception block
				{
					if(exception->type == Value::Exception)
						return;

					auto error = (ReturnException *)exception;

					if(error->target == frame->block)
					{
						switch(error->type)
						{
							case Value::ReturnException:
								handle_return(frame, 0, error->value);
								break;

							case Value::BreakException:
								handle_break(frame, error->value, static_cast<BreakException *>(exception)->id);
								break;

							default:
								break;
						}
					}


					return;
				}

				ExceptionBlock *block = frame->block->exception_blocks[frame->block_index];

				switch(exception->type)
				{
					case Value::BreakException:
					{
						auto error = (BreakException *)exception;

						if(error->target == frame->block)
							handle_break(frame,  error->value, error->id);
						break;
					}

					case Value::ReturnException:
					{
						auto error = (ReturnException *)exception;

						if(error->target == frame->block)
							handle_return(frame, block, error->value);
						break;
					}

					case Value::Exception:
					{
						ExceptionBlock *current_block = block;

						while(current_block)
						{
							for(auto i = current_block->handlers.begin(); i != current_block->handlers.end(); ++i)
							{
								switch(i()->type)
								{
									case RuntimeException:
										rescue(frame, block, current_block, ((RuntimeExceptionHandler *)i())->rescue_label.address);

									default:
										break;
								}
							}

							current_block = current_block->parent;
						}
					}
					break;

					default:
						mirb_debug_abort("Unknown exception type");
				}

				local_unwind(frame, block, 0);
			}
		};
	};
};


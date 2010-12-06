#include "support.hpp"
#include "../../support.hpp"
#include "../../compiler.hpp"
#include "../../runtime.hpp"
#include "../../classes/proc.hpp"
#include "../../classes/module.hpp"

namespace Mirb
{
	namespace Arch
	{
		namespace Support
		{
			Frame *current_frame;

			CharArray backtrace()
			{
				Frame *current = current_frame;

				CharArray result;

				auto list_block = [&](FramePrefix *block)
				{
					if(result.length != 0)
						result += "\n";

					value_t module = block->module;

					if(Value::type(module) == Value::IClass)
						module = cast<Module>(module)->instance_of;
					
					result += CharArray(inspect_object(module));

					if(block->object != module)
						result += "(" + CharArray(inspect_object(block->object)) + ")";

					result += "#" + block->name->string + " at <unknown>";
				};
				
				while(current)
				{
					mirb_debug_assert(current->type == Frame::NativeEntry);
					
					NativeEntry *native_entry = (NativeEntry *)current;

					list_block(&native_entry->prefix);

					FramePrefix *block = native_entry->prefix.prev();

					mirb_debug_assert(block);

					do
					{
						list_block(block);

						block = block->prev();
					}
					while(block);

					do
					{
						current = current->prev;
					}
					while(current && current->type == Frame::Exception);
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
				
			value_t __cdecl interpolate(size_t argc, value_t argv[])
			{
				return Mirb::Support::interpolate(argc, argv);
			}
			
			value_t __stdcall get_const(value_t obj, Symbol *name)
			{
				return Mirb::Support::get_const(obj, name);
			}

			void __stdcall set_const(value_t obj, Symbol *name, value_t value)
			{
				return Mirb::Support::set_const(obj, name, value);
			}
			
			value_t __stdcall get_ivar(value_t obj, Symbol *name)
			{
				return Mirb::Support::get_ivar(obj, name);
			}

			void __stdcall set_ivar(value_t obj, Symbol *name, value_t value)
			{
				return Mirb::Support::set_ivar(obj, name, value);
			}

			value_t __cdecl create_array(size_t argc, value_t argv[])
			{
				return Mirb::Support::create_array(argc, argv);
			}
			
			compiled_block_t __fastcall jit_invoke(Block *block) mirb_external("mirb_arch_support_jit_invoke");

			compiled_block_t __fastcall jit_invoke(Block *block)
			{
				return Compiler::defered_compile(block);
			}

			#ifdef _MSC_VER
				__declspec(naked) value_t __fastcall call(value_t block, value_t dummy, value_t obj, Symbol *method_name, size_t argc, value_t argv[])
				{
					__asm
					{
						push ecx
						sub esp, 4
						push esp
						push dword ptr [esp + 20]
						push dword ptr [esp + 20]
						call lookup
						pop edx
						pop ecx
						jmp eax
					}
				}

				__declspec(naked) value_t __fastcall super(value_t block, value_t method_module, value_t obj, Symbol *method_name, size_t argc, value_t argv[])
				{
					__asm
					{
						push ecx
						sub esp, 4
						push esp
						push dword ptr [esp + 20]
						push edx
						call lookup_super
						pop edx
						pop ecx
						jmp eax
					}
				}
			
				value_t ruby_call(compiled_block_t code, value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;

					__asm
					{
						push ebp
						push argv
						push argc
						push name
						push obj
						mov ecx, block
						mov edx, module
						mov ebx, code
						xor ebp, ebp
						call ebx
						pop ebp

						mov result, eax
					}

					return result;
				}

				value_t closure_call(compiled_block_t code, value_t *scopes[], value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;

					__asm
					{
						push ebp
						push argv
						push argc
						push name
						push obj
						mov eax, scopes
						mov ecx, block
						mov edx, module
						mov ebx, code
						xor ebp, ebp
						call ebx
						pop ebp

						mov result, eax
					}

					return result;
				}

				__declspec(naked) void jit_stub()
				{
					__asm
					{
						push ebx

						push eax
						push ecx
						push edx
						mov ecx, [esp + 16]
						call jit_invoke
						mov ebx, eax
						pop edx
						pop ecx
						pop eax

						push [esp + 24] // argv
						push [esp + 24] // argc
						push [esp + 24] // name
						push [esp + 24] // obj
						call ebx

						pop ebx
						add esp, 4
						ret 16
					}
				}
			#else
				value_t ruby_call(compiled_block_t code, value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;
					
					asm ("pushl %%ebp\n"
						"pushl %[argv]\n"
						"pushl %[argc]\n"
						"pushl %[name]\n"
						"pushl %[obj]\n"
						"xorl %%ebp, %%ebp\n"
						"call *%[code]\n"
						"popl %%ebp\n"
					: "=a"(result)
					: [argv] "g"(argv), [argc] "g"(argc), [name] "g"(name), [obj] "g"(obj), [code] "r"(code), "c"(block), "d"(module)
					: "memory");
					
					return result;
				}

				value_t closure_call(compiled_block_t code, value_t *scopes[], value_t obj, Symbol *name, value_t module, value_t block, size_t argc, value_t argv[])
				{
					value_t result;
					
					asm ("pushl %%ebp\n"
						"pushl %[argv]\n"
						"pushl %[argc]\n"
						"pushl %[name]\n"
						"pushl %[obj]\n"
						"xorl %%ebp, %%ebp\n"
						"call *%[code]\n"
						"popl %%ebp\n"
					: "=a"(result)
					: [argv] "g"(argv), [argc] "g"(argc), [name] "g"(name), [obj] "g"(obj), [code] "r"(code), "a"(scopes), "c"(block), "d"(module)
					: "memory");
					
					return result;
				}
			#endif
			
			value_t __cdecl create_closure(Block *block, value_t self, size_t argc, value_t *argv[])
			{
				return Mirb::Support::create_closure(block, self, argc, argv);
			}
			
			compiled_block_t __stdcall lookup(value_t obj, Symbol *name, value_t *result_module)
			{
				return Mirb::lookup(obj, name, result_module);
			}

			compiled_block_t __stdcall lookup_super(value_t module, Symbol *name, value_t *result_module)
			{
				return Mirb::lookup_super(module, name, result_module);
			}
			
			void __noreturn exception_raise(ExceptionData *data)
			{
				Frame *frame = current_frame;

				while(frame)
				{
					switch(frame->type)
					{
						case Frame::Exception:
							handle_exception((ExceptionFrame *)frame, data);
					};

					frame = frame->prev;
				}

				mirb_runtime_abort("Unable to find a exception handler");
			}

			void __noreturn __stdcall far_return(value_t value, Block *target)
			{
				ExceptionData data;

				data.type = ReturnException;
				data.target = target;
				data.value = value;

				exception_raise(&data);
			}

			void __noreturn __stdcall far_break(value_t value, Block *target, size_t id)
			{
				ExceptionData data;

				data.type = BreakException;
				data.target = target;
				data.value = value;
				data.id = id;

				exception_raise(&data);
			}

			static void local_unwind(ExceptionFrame *frame, ExceptionBlock *block, ExceptionBlock *target)
			{
				frame->handling = 1;

				size_t ebp_value = (size_t)&frame->prefix.prev_ebp;
				
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

						#ifdef _MSC_VER
							__asm
							{
								push ebx
								push esi
								push edi
								push ebp

								mov eax, target_label
								mov ebp, ebp_value
								call eax
								
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
							: "esi", "edi", "edx", "memory");
						#endif
					}

					block = block->parent;
				}

				frame->handling = 0;
			}
			
			static void __noreturn jump_to_handler(ExceptionFrame *frame, void *target, value_t value)
			{
				current_frame = frame;

				size_t ebp_value = (size_t)&frame->prefix.prev_ebp;
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
			
			void handle_exception(ExceptionFrame *frame, ExceptionData *data)
			{
				if(frame->block_index == (size_t)-1) // Outside any exception block
				{
					if(data->target == frame->block)
					{
						switch(data->type)
						{
							case ReturnException:
								handle_return(frame, 0, data->value);
								break;

							case BreakException:
								handle_break(frame, data->value, data->id);
								break;

							default:
								break;
						}
					}

					return;
				}

				ExceptionBlock *block = frame->block->exception_blocks[frame->block_index];

				switch(data->type)
				{
					case BreakException:
					{
						if(data->target == frame->block)
							handle_break(frame,  data->value, data->id);
						break;
					}

					case ReturnException:
					{
						if(data->target == frame->block)
							handle_return(frame, block, data->value);
						break;
					}

					case RubyException:
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


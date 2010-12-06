extern "C"
{
	#include <udis86.h>
};

#include "../../generic/mem_stream.hpp"
#include "disassembly.hpp"
#include "support.hpp"

namespace Mirb
{
	namespace Arch
	{
		namespace Disassembly
		{
			#define MIRB_SYMBOL(name) {(void *)&name, #name}
			
			static Symbol symbols[] = {
				MIRB_SYMBOL(Arch::Support::current_frame),
				MIRB_SYMBOL(Arch::Support::far_return),
				MIRB_SYMBOL(Arch::Support::far_break),
				MIRB_SYMBOL(Arch::Support::create_heap),
				MIRB_SYMBOL(Arch::Support::create_closure),
				MIRB_SYMBOL(Arch::Support::create_array),
				MIRB_SYMBOL(Arch::Support::interpolate),
				MIRB_SYMBOL(Arch::Support::get_const),
				MIRB_SYMBOL(Arch::Support::set_const),
				MIRB_SYMBOL(Arch::Support::get_ivar),
				MIRB_SYMBOL(Arch::Support::set_ivar),
				MIRB_SYMBOL(Arch::Support::define_string),
				MIRB_SYMBOL(Arch::Support::define_class),
				MIRB_SYMBOL(Arch::Support::define_module),
				MIRB_SYMBOL(Arch::Support::define_method),
				MIRB_SYMBOL(Arch::Support::call),
				MIRB_SYMBOL(Arch::Support::super),
			};

			const char *find_symbol(void *address, Vector<Symbol *, MemoryPool> *symbols)
			{
				for(size_t i = 0; i < sizeof(Disassembly::symbols) / sizeof(Symbol); i++)
				{
					if(Disassembly::symbols[i].address == address)
						return Disassembly::symbols[i].symbol;
				}

				if(symbols)
				{
					for(auto i = symbols->begin(); i != symbols->end(); ++i)
					{
						if(i()->address == address)
							return i()->symbol;
					}
				}

				return 0;
			}

			void dump_hex(unsigned char* address, int length)
			{
				int min_length = 7 - length;

				int index = 0;

				while(index < length)
					printf(" %.2X", (size_t)*(address + index++));

				while(min_length-- > 0)
					std::cout << "   ";
			}

			void dump_instruction(ud_t* ud_obj, Vector<Symbol *, MemoryPool> *symbols)
			{
				printf("0x%08X:", (size_t)ud_insn_off(ud_obj));

				dump_hex(ud_insn_ptr(ud_obj), ud_insn_len(ud_obj));

				std::cout << " " << ud_insn_asm(ud_obj);
	
				const char *symbol = 0;
	
				for(size_t i = 0; !symbol && i < 3; i++)
				{
					switch(ud_obj->operand[i].type)
					{
						case UD_OP_IMM:
						case UD_OP_MEM:
							symbol = find_symbol((void *)ud_obj->operand[i].lval.udword, symbols);
							break;
			
						case UD_OP_PTR:
							symbol = find_symbol((void *)ud_obj->operand[i].lval.ptr.off, symbols);
							break;
			
						case UD_OP_JIMM:
							symbol = find_symbol((void *)((size_t)ud_insn_off(ud_obj) + ud_obj->operand[i].lval.udword + ud_insn_len(ud_obj)), symbols);
							break;
			
						default:
							break;
					}
				}
	
				if(symbol)
					std::cout << " ; " << symbol;
	
				std::cout << "\n";
			}

			void dump_code(MemStream &stream, Vector<Symbol *, MemoryPool> *symbols)
			{
				ud_t ud_obj;

				ud_init(&ud_obj);
				ud_set_input_buffer(&ud_obj, (uint8_t *)stream.pointer(), stream.size());
				ud_set_mode(&ud_obj, 32);
				ud_set_syntax(&ud_obj, UD_SYN_INTEL);
				ud_set_pc(&ud_obj, (size_t)stream.pointer());

				while(ud_disassemble(&ud_obj))
				{
					dump_instruction(&ud_obj, symbols);
				}
			}
		};
	};
};

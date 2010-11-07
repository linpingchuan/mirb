#pragma once
#include "../common.hpp"
#include "../memory-pool.hpp"
#include "../list.hpp"
#include "../vector.hpp"
#include "../bit-set-wrapper.hpp"
#include "opcodes.hpp"
#include "../tree/tree.hpp"

struct exception_block;

namespace Mirb
{
	class Block;
	
	namespace Tree
	{
		class Variable;
		class Scope;
	};
	
	namespace CodeGen
	{
		class BasicBlock;
		
		class BasicBlock
		{
			public:
				BasicBlock(MemoryPool &memory_pool, Block &block);

				#ifdef DEBUG
					size_t id;
				#endif

				Entry<BasicBlock> entry;
				Entry<BasicBlock> work_list_entry;

				size_t var_count; //TODO: Remove this

				bool in_work_list;
				
				bit_set_t def_bits;
				bit_set_t use_bits;
				
				bit_set_t in;
				bit_set_t out;

				Vector<BasicBlock *, MemoryPool> pred_blocks;

				SimpleList<Opcode> opcodes; // A linked list of opcodes
				
				BasicBlock *next_block;
				BasicBlock *branch_block;

				void prepare_liveness(BitSetWrapper<MemoryPool> &w, size_t var_count);
				
				template<typename T> struct Use
				{
					static void func(BasicBlock &block, T &opcode)
					{
						opcode.use([&](Tree::Variable *var) {
							if(!BitSetWrapper<MemoryPool>::get(block.def_bits, var->index))
								BitSetWrapper<MemoryPool>::set(block.use_bits, var->index);
						});
					}
				};

				template<typename T> struct Def
				{
					static void func(BasicBlock &block, T &opcode)
					{
						opcode.def([&](Tree::Variable *var) {
							BitSetWrapper<MemoryPool>::set(block.def_bits, var->index);
						});
					}
				};

				void next(BasicBlock *block)
				{
					next_block = block;
					block->pred_blocks.push(this);
				}

				void branch(BasicBlock *block)
				{
					branch_block = block;
					block->pred_blocks.push(this);
				}
		};

		class Block
		{
			public:
				Mirb::Block *final;
				BasicBlock *prolog; // The point after the prolog of the block.
				BasicBlock *epilog; // The end of the block
				
				/*
				 * Exception related fields
				 */
				struct exception_block *current_exception_block;
				size_t current_exception_block_id;

				size_t var_count;
				
				Tree::Variable *self_var;
				Tree::Variable *return_var;
				
				#ifdef DEBUG
					size_t basic_block_count; // Nicer basic block labeling...
				#endif

				void analyse_liveness(MemoryPool &memory_pool, Tree::Scope *scope);
				
				Block();
				
				List<BasicBlock> basic_blocks; // A linked list of basic blocks
		};
	};
};

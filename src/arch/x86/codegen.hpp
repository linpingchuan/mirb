#pragma once
#include "x86.hpp"
#include "../../codegen/bytecode.hpp"
#include "../../codegen/opcodes.hpp"
#include "../../mem_stream.hpp"

namespace Mirb
{
	class MemoryPool;
	
	namespace Tree
	{
		class Variable;
	};

	namespace CodeGen
	{
		class Block;

		class NativeMeasurer
		{
			public:
				static size_t measure(Block *block);
		};

		class NativeGenerator
		{
			private:
				MemStream &stream;
				Block *block;
				size_t index;

				size_t reg_low(size_t reg);
				size_t reg_high(size_t reg);
				int stack_offset(Tree::Variable *var);
				int stack_offset(size_t loc);
				void stack_pop(size_t count);
				void rex(size_t r, size_t x, size_t b);
				void modrm(size_t mod, size_t reg, size_t rm);
				void stack_modrm(size_t reg, Tree::Variable *var);
				void mov_imm_to_reg(size_t imm, size_t reg);
				void mov_imm_to_var(size_t imm, Tree::Variable *var);
				void mov_reg_to_reg(size_t src, size_t dst);
				void mov_reg_to_var(size_t reg, Tree::Variable *var);
				void mov_var_to_reg(Tree::Variable *var, size_t reg);
				void push_reg(size_t reg);
				void push_var(Tree::Variable *var);
				void push_imm(size_t imm);
				void test_var(Tree::Variable *var);
				void mov_arg_to_reg(size_t arg, size_t reg);
				void mov_arg_to_var(size_t arg, Tree::Variable *var);
				
				template<typename T> void call(T *func)
				{
					stream.b(0xE8);
					stream.d((size_t)func - ((size_t)stream.position + 4));
				}

				template<size_t reg> void preserve_reg()
				{
					if(!BitSetWrapper<MemoryPool>::get(block->used_registers, Arch::Register::DI))
						return;

					size_t index = Arch::Register::to_virtual(reg);

					if(index == (size_t)Arch::Register::None || index < Arch::caller_save_start)
						return;

					rex(reg_high(reg), 0, 0);
					stream.b(0x89);
					modrm(1, reg, Arch::Register::BP);
					stream.b((int8_t)stack_offset(block->stack_vars + (index - Arch::caller_save_start)));
				}

				template<size_t reg> void restore_reg()
				{
					if(!BitSetWrapper<MemoryPool>::get(block->used_registers, Arch::Register::DI))
						return;

					size_t index = Arch::Register::to_virtual(reg);

					if(index == (size_t)Arch::Register::None || index < 3)
						return;

					rex(reg_high(reg), 0, 0);
					stream.b(0x8B);
					modrm(1, reg, Arch::Register::BP);
					stream.b((int8_t)stack_offset(block->stack_vars + (index - 3)));
				}

				void preserve_reg(size_t reg);
				void restore_reg(size_t reg);
				void preserve();
				void restore();

				template<typename T> void preserve_regs(T code)
				{
					preserve();
					code();
					restore();
				}

				SimplerList<BranchOpcode, BranchOpcode, &BranchOpcode::branch_entry> branch_list;
			public:
				NativeGenerator(MemStream &stream, MemoryPool &memory_pool) : stream(stream) {}

				static const size_t long_mode = false;
				static const size_t spare = Arch::Register::AX;

				template<typename T> void generate(T &op)
				{
					debug_fail("Not implemented");
				}

				void generate(Block *block);
		};
		
		template<> void NativeGenerator::generate(MoveOp &op);
		template<> void NativeGenerator::generate(LoadOp &op);
		template<> void NativeGenerator::generate(LoadRawOp &op);
		template<> void NativeGenerator::generate(PushOp &op);
		template<> void NativeGenerator::generate(PushImmediateOp &op);
		template<> void NativeGenerator::generate(PushRawOp &op);
		template<> void NativeGenerator::generate(ClosureOp &op);
		template<> void NativeGenerator::generate(CallOp &op);
		template<> void NativeGenerator::generate(BranchIfOp &op);
		template<> void NativeGenerator::generate(BranchUnlessOp &op);
		template<> void NativeGenerator::generate(BranchOp &op);
		template<> void NativeGenerator::generate(ReturnOp &op);
	};
};

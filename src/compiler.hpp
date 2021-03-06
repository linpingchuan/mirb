#pragma once
#include "common.hpp"
#include "generic/memory-pool.hpp"
#include "block.hpp"

namespace Mirb
{
	namespace Tree
	{
		class Scope;
	};
	
	class Block;
	class Compiler
	{
		private:
		public:
			static value_t deferred_block(Frame &frame);

			static Block *compile(Tree::Scope *scope, MemoryPool memory_pool);
			static Block *defer(Tree::Scope *scope);
	};
};

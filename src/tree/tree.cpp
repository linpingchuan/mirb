#include "tree.hpp"

namespace Mirb
{
	namespace Tree
	{
		Scope::Scope(Document *document, Fragment &fragment, Scope *parent, Type type) :
			ObjectHeader(Value::InternalScope),
			document(document),
			final(0),
			fragment(&fragment),
			type(type),
			parent(parent),
			break_id(no_break_id),
			break_targets(0),
			break_dst(no_var),
			require_exceptions(false),
			variables(2, fragment),
			heap_vars(0),
			block_parameter(0),
			referenced_scopes(fragment),
			variable_list(2, fragment),
			zsupers(fragment)
		{
			owner = parent ? parent->owner : 0;
		}

		void Scope::require_scope(Scope *scope)
		{
			if(referenced_scopes.find(scope))
				return;
			
			referenced_scopes.push(scope);
		}
		
		void Scope::require_args(Scope *owner)
		{
			for(auto i = owner->parameters.begin(); i != owner->parameters.end(); ++i)
			{
				require_var(owner, *i);
			}
			
			if(owner->block_parameter)
				require_var(owner, owner->block_parameter);
		}

		void Scope::require_var(Scope *owner, Variable *var)
		{
			if(var->type != Variable::Heap)
			{
				var->type = Variable::Heap;
				var->loc = owner->heap_vars++;
				var->owner = owner;
			}
			
			/*
			 * Make sure the block owning the varaible is required by the current block and parents.
			 */
			
			Scope *current = this;
			
			while(current != owner)
			{
				current->require_scope(owner);
				
				current = current->parent;
			}
		}

		Scope *Scope::defined(Symbol *name, bool recursive)
		{
			if(variables.get(name))
				return this;
			
			Scope *current = this;
			
			if(recursive)
			{
				while(current->type == Scope::Closure)
				{
					current = current->parent;
					
					if(current->variables.get(name))
						return current;
				}
			}
			
			return 0;
		}
		
		Fragment::Fragment(Fragment *parent, size_t chunk_size) : fragments(*this), chunk_size(chunk_size)
		{
			if(prelude_likely(parent != 0))
				parent->fragments.push(this);
			
			current = Chunk::create(chunk_size);
			chunks.append(current);
		}
		
		void *Fragment::reallocate(void *memory, size_t old_size, size_t new_size)
		{
			void *result = allocate(new_size);
			
			memcpy(result, memory, old_size);
			
			return result;
		}
		
		void *Fragment::allocate(size_t bytes)
		{
			void *result = current->allocate(bytes);
			
			if(prelude_unlikely(!result))
			{
				Chunk *chunk;
				
				if(prelude_unlikely(bytes > Chunk::allocation_limit))
				{
					chunk = Chunk::create(bytes);
				}
				else
				{
					chunk = Chunk::create(chunk_size);
					current = chunk;
				}
				
				chunks.append(chunk);
				
				result = chunk->allocate(bytes);
				
				assert(result);
			}
			
			return result;
		}
		
		Chunk *Chunk::create(size_t size)
		{
			uint8_t *memory = (uint8_t *)malloc(sizeof(Chunk) + size);
			
			assert(memory);
			
			Chunk *result = new (memory) Chunk;
			
			result->current = memory + sizeof(Chunk);
			result->end = result->current + size;
			
			return result;
		}
		
		void *Chunk::allocate(size_t bytes)
		{
			uint8_t *result;
			
			#ifdef VALGRIND
				result = (uint8_t *)malloc(bytes);
				
				assert(result);
			#else
				result = current;
				
				uint8_t *next = result + bytes;
				
				if(prelude_unlikely(next > end))
					return 0;
				
				current = next;
			#endif
			
			return (void *)result;
		}
	};
};

void *operator new(size_t bytes, Mirb::Tree::Fragment *fragment) throw()
{
	return fragment->allocate(bytes);
}

void operator delete(void *, Mirb::Tree::Fragment *fragment) throw()
{
}

void *operator new[](size_t bytes, Mirb::Tree::Fragment *fragment) throw()
{
	return fragment->allocate(bytes);
}

void operator delete[](void *, Mirb::Tree::Fragment *fragment) throw()
{
}

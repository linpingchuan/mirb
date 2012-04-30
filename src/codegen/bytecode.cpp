#include "bytecode.hpp"
#include "../tree/nodes.hpp"
#include "../tree/tree.hpp"
#include "../compiler.hpp"
#include "../block.hpp"
#include "../classes/fixnum.hpp"

#ifdef MIRB_DEBUG_COMPILER
	#include "printer.hpp"
#endif

namespace Mirb
{
	namespace CodeGen
	{
		VariableGroup::VariableGroup(ByteCodeGenerator *bcg, size_t size) : bcg(bcg), size(size)
		{
			if(size)
			{
				var = bcg->var_count;
				bcg->var_count += size;
			}
		}

		var_t VariableGroup::operator[](size_t index)
		{
			if(size)
			{
				return var + index;
			}
			else
			{
				return no_var;
			}
		}
		
		var_t VariableGroup::use()
		{
			if(size)
			{
				// TODO:  return unused variables to the list

				return var;
			}
			else
			{
				return no_var;
			}
		}

		void (ByteCodeGenerator::*ByteCodeGenerator::jump_table[Tree::SimpleNode::Types])(Tree::Node *basic_node, var_t var) = {
			0, // None
			&ByteCodeGenerator::convert_string,
			&ByteCodeGenerator::convert_interpolated_string,
			0, // InterpolatedPair
			&ByteCodeGenerator::convert_integer,
			&ByteCodeGenerator::convert_variable,
			&ByteCodeGenerator::convert_ivar,
			&ByteCodeGenerator::convert_constant,
			&ByteCodeGenerator::convert_unary_op,
			&ByteCodeGenerator::convert_boolean_not,
			&ByteCodeGenerator::convert_binary_op,
			&ByteCodeGenerator::convert_boolean_op,
			&ByteCodeGenerator::convert_assignment,
			&ByteCodeGenerator::convert_self,
			&ByteCodeGenerator::convert_nil,
			&ByteCodeGenerator::convert_true,
			&ByteCodeGenerator::convert_false,
			&ByteCodeGenerator::convert_array,
			0, // Block
			0, // Invoke
			&ByteCodeGenerator::convert_call,
			&ByteCodeGenerator::convert_super,
			&ByteCodeGenerator::convert_if,
			&ByteCodeGenerator::convert_group,
			0, // Void
			&ByteCodeGenerator::convert_return,
			&ByteCodeGenerator::convert_next,
			&ByteCodeGenerator::convert_break,
			&ByteCodeGenerator::convert_redo,
			&ByteCodeGenerator::convert_class,
			&ByteCodeGenerator::convert_module,
			&ByteCodeGenerator::convert_method,
			0, // Rescue
			&ByteCodeGenerator::convert_handler
		};
		
		void ByteCodeGenerator::convert_string(Tree::Node *basic_node, var_t var)
		{
			if(is_var(var))
			{
				auto node = (Tree::StringNode *)basic_node;
				
				gen_string(var, node->string);
			}
		}
		
		void ByteCodeGenerator::convert_interpolated_string(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::InterpolatedStringNode *)basic_node;
			
			size_t param_count = 0;

			for(auto i = node->pairs.begin(); i != node->pairs.end(); ++i)
			{
				if(i().string.length)
					param_count++;
				
				param_count++;
			}
			
			if(node->tail.length)
				param_count++;
			
			VariableGroup group(this, param_count);

			size_t param = 0;
			
			for(auto i = node->pairs.begin(); i != node->pairs.end(); ++i)
			{
				if(i().string.length)
					gen_string(group[param++], i().string);
				
				to_bytecode(i().group, group[param++]);
			}
			
			if(node->tail.length)
				gen_string(group[param++], node->tail);
			
			gen<InterpolateOp>(var, group.size, group.use());
		}
		
		void ByteCodeGenerator::convert_integer(Tree::Node *basic_node, var_t var)
		{
			if(is_var(var))
			{
				auto node = (Tree::IntegerNode *)basic_node;
				
				gen<LoadFixnumOp>(var, Fixnum::from_int(node->value));
			}
		}

		var_t ByteCodeGenerator::read_variable(Tree::Variable *var)
		{
			if(var->type == Tree::Variable::Heap)
			{
				var_t heap;

				if(var->owner != scope)
				{
					heap = create_var();

					gen<LookupOp>(heap, scope->referenced_scopes.index_of(var->owner));
				}
				else
					heap = heap_var;

				var_t result = create_var();

				gen<GetHeapVarOp>(result, heap, var->loc);

				return result;
			}
			
			return ref(var);
		}

		void ByteCodeGenerator::convert_variable(Tree::Node *basic_node, var_t var)
		{
			if(!is_var(var))
				return;

			auto node = (Tree::VariableNode *)basic_node;

			gen<MoveOp>(var, read_variable(node->var));
		}
		
		void ByteCodeGenerator::convert_ivar(Tree::Node *basic_node, var_t var)
		{
			if(!is_var(var))
				return;
			
			auto node = (Tree::IVarNode *)basic_node;
			
			gen<GetIVarOp>(var, node->name);
		}
		
		void ByteCodeGenerator::convert_constant(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::ConstantNode *)basic_node;
			
			if(node->obj)
				to_bytecode(node->obj, var);
			else if(is_var(var))
				gen<LoadObjectOp>(var);
			
			gen<GetConstOp>(var, var, node->name);
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_unary_op(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::UnaryOpNode *)basic_node;
			
			var_t temp = reuse(var);
			
			to_bytecode(node->value, temp);
			
			gen<CallOp>(var, temp, Symbol::from_string(Lexeme::names[node->op].c_str()), no_var, (Mirb::Block *)0, (size_t)0, no_var);
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_boolean_not(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::BooleanNotNode *)basic_node;
			
			if(is_var(var))
			{
				var_t temp = reuse(var);
				
				to_bytecode(node->value, temp);
				
				Label *label_true = create_label();
				Label *label_end = create_label();
				Label *label_false = create_label();
				
				gen_if(label_true, temp);

				gen(label_false);
				gen<LoadTrueOp>(var);
				gen_branch(label_end);

				gen(label_true);
				gen<LoadFalseOp>(var);
				
				gen(label_end);
			}
			else
				to_bytecode(node->value, no_var);
		}
		
		void ByteCodeGenerator::convert_binary_op(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::BinaryOpNode *)basic_node;
			
			if(node->op == Lexeme::LOGICAL_AND || node->op == Lexeme::LOGICAL_OR)
			{
				convert_boolean_op(basic_node, var);
				return;
			}
			
			var_t left = reuse(var);
			
			to_bytecode(node->left, left);

			VariableGroup group(this, 1);
			
			to_bytecode(node->right, group[0]);
			
			gen<CallOp>(var, left, Symbol::from_string(Lexeme::names[node->op].c_str()), no_var, (Mirb::Block *)0, group.size, group.use());
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_boolean_op(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::BinaryOpNode *)basic_node;
			
			to_bytecode(node->left, var);
			
			Label *label_end = create_label();
			Label *body = create_label();
			
			if(node->op == Lexeme::KW_OR || node->op == Lexeme::LOGICAL_OR)
				gen_if(label_end, var);
			else
				gen_unless(label_end, var);

			gen(body);
			
			to_bytecode(node->right, var);
			
			gen(label_end);
		}

		void ByteCodeGenerator::write_variable(Tree::Variable *var, var_t value)
		{
			if(var->type == Tree::Variable::Heap)
			{
				var_t heap;

				if(var->owner != scope)
				{
					heap = create_var();

					gen<LookupOp>(heap, scope->referenced_scopes.index_of(var->owner));
				}
				else
					heap = heap_var;

				gen<SetHeapVarOp>(heap, var->loc, value);
			}
			else
			{
				gen<MoveOp>(ref(var), value);
			}
		}
		
		void ByteCodeGenerator::convert_assignment(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::AssignmentNode *)basic_node;
			
			switch(node->left->type())
			{
				case Tree::SimpleNode::Variable:
				{
					auto variable = (Tree::VariableNode *)node->left;

					if(variable->var->type == Tree::Variable::Heap)
					{
						var_t temp = reuse(var);

						to_bytecode(node->right, temp);
						
						write_variable(variable->var, temp);

						if(is_var(var))
							gen<MoveOp>(var, temp);
					}
					else
					{
						to_bytecode(node->right, ref(variable->var));
					
						if(is_var(var))
							gen<MoveOp>(var, ref(variable->var));
					}
					
					return;
				}
				
				case Tree::SimpleNode::IVar:
				{
					auto variable = (Tree::IVarNode *)node->left;
					
					var_t temp = reuse(var);
					
					to_bytecode(node->right, temp);
						
					gen<SetIVarOp>(variable->name, temp);

					if(is_var(var))
						gen<MoveOp>(var, temp);
					
					return;
				}
				
				case Tree::SimpleNode::Constant:
				{
					auto variable = (Tree::ConstantNode *)node->left;
					
					var_t value = reuse(var);
					var_t obj = create_var();
					
					if(variable->obj)
						to_bytecode(variable->obj, obj);
					else
						gen<LoadObjectOp>(obj);
					
					to_bytecode(node->right, value);
					
					gen<SetConstOp>(obj, variable->name, value);
					location(variable->range);

					if(is_var(var))
						gen<MoveOp>(var, value);
					
					return;	
				}	
				
				default:
					mirb_debug_abort("Unknown left hand expression");
			}
		}
		
		void ByteCodeGenerator::convert_self(Tree::Node *, var_t var)
		{
			if(is_var(var))
				gen<MoveOp>(var, self_var);
		}
		
		void ByteCodeGenerator::convert_nil(Tree::Node *, var_t var)
		{
			if(is_var(var))
				gen<LoadNilOp>(var);
		}
		
		void ByteCodeGenerator::convert_true(Tree::Node *, var_t var)
		{
			if(is_var(var))
				gen<LoadTrueOp>(var);
		}
		
		void ByteCodeGenerator::convert_false(Tree::Node *, var_t var)
		{
			if(is_var(var))
				gen<LoadFalseOp>(var);
		}
		
		void ByteCodeGenerator::convert_array(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::ArrayNode *)basic_node;
			
			if(!is_var(var))
			{
				for(auto i = node->entries.begin(); i != node->entries.end(); ++i)
					to_bytecode(*i, 0);
				
				return;
			}
			
			size_t param = 0;

			VariableGroup group(this, node->entries.size);
			
			for(auto i = node->entries.begin(); i != node->entries.end(); ++i)
			{
				to_bytecode(*i, group[param++]);
			}
			
			gen<ArrayOp>(var, group.size, group.use());	
		}
		
		void ByteCodeGenerator::convert_call(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::CallNode *)basic_node;
			
			var_t obj = reuse(var);
			
			to_bytecode(node->object, obj);
			
			size_t argc;
			var_t argv;

			var_t closure = call_args(node->arguments, node->block ? node->block->scope : nullptr, argc, argv, var);
			
			gen<CallOp>(var, obj, node->method, closure, node->block ? node->block->scope->final : nullptr, argc, argv);
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_super(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::SuperNode *)basic_node;
			
			if(node->pass_args)
			{
				var_t closure = node->block ? block_arg(node->block->scope, var) : ref(scope->owner->block_parameter);
				
				// push arguments

				VariableGroup group(this, scope->owner->parameters.size);
				
				size_t param = 0;
				
				for(auto i = scope->owner->parameters.begin(); i != scope->owner->parameters.end(); ++i)
					gen<MoveOp>(group[param++], ref(*i));
				
				gen<SuperOp>(var, closure, node->block ? node->block->scope->final : nullptr, group.size, group.use());
				location(node->range);
			}
			else
			{
				size_t argc;
				var_t argv;

				var_t closure = call_args(node->arguments, scope, argc, argv, var);
				
				gen<SuperOp>(var, closure, node->block ? node->block->scope->final : nullptr, argc, argv);
				location(node->range);
			}
		}
		
		void ByteCodeGenerator::convert_if(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::IfNode *)basic_node;
			
			var_t temp = reuse(var);

			Label *label_else = create_label();
			Label *label_end = create_label();
			Label *body = create_label();
			
			to_bytecode(node->left, temp);
			
			if(node->inverted)
				gen_if(label_else, temp);
			else
				gen_unless(label_else, temp);
			
			gen(body);
			
			if(is_var(var))
			{
				var_t result_left = create_var();
				var_t result_right = create_var();

				to_bytecode(node->middle, result_left);
				
				gen<MoveOp>(var, result_left);
				gen_branch(label_end);

				gen(label_else);

				to_bytecode(node->right, result_right);
				
				gen<MoveOp>(var, result_right);
				gen(label_end);
			}
			else
			{
				to_bytecode(node->middle, no_var);
				
				gen_branch(label_end);

				gen(label_else);

				to_bytecode(node->right, no_var);

				gen(label_end);
			}
		}
		
		void ByteCodeGenerator::convert_group(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::GroupNode *)basic_node;
			
			if(node->statements.empty())
			{
				if (is_var(var))
					gen<LoadNilOp>(var);
				
				return;
			}
			
			for(auto i = node->statements.begin(); i != node->statements.end(); ++i)
			{
				to_bytecode(*i, i().entry.next ? no_var : var);
			}
		}
		
		void ByteCodeGenerator::convert_return(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::ReturnNode *)basic_node;
			
			var_t temp = reuse(var);
			
			to_bytecode(node->value, temp);
			
			if(scope->type == Tree::Scope::Closure)
			{
				// TODO: only raise if this is a proc, not lambda
				
				//rt_value label = create_label();

				//block_push(block, B_TEST_PROC, 0, 0, 0);
				//block_push(block, B_JMPNE, label, 0, 0);
				gen<UnwindReturnOp>(temp, scope->owner->final);
				location(node->range);

				//block_emmit_label(block, label);
			}
			else if(in_ensure_block())
			{
				gen<UnwindReturnOp>(temp, final);
				location(node->range);
			}
			else
			{
				gen<MoveOp>(return_var, temp);
				branch(epilog);
			}
		}
		
		void ByteCodeGenerator::convert_break(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::BreakNode *)basic_node;
			
			var_t temp = reuse(var);
			
			to_bytecode(node->value, temp);

			gen<UnwindBreakOp>(temp, scope->parent->final, scope->break_dst);
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_next(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::NextNode *)basic_node;
			
			var_t temp = reuse(var);

			
			if(in_ensure_block())
				gen<UnwindNextOp>(return_var);
			else
			{
				to_bytecode(node->value, temp);
			
				gen<MoveOp>(return_var, temp);
				branch(epilog);
			}
		}
		
		void ByteCodeGenerator::convert_redo(Tree::Node *, var_t)
		{
			if(in_ensure_block())
				gen_unwind_redo(body);
			else
				branch(body);
		}
		
		void ByteCodeGenerator::convert_class(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::ClassNode *)basic_node;
			
			var_t super;
			
			if(node->super)
			{
				super = reuse(var);
				
				to_bytecode(node->super, super);
			}
			else
				super = no_var;
			
			gen<ClassOp>(var, node->name, super, compile(node->scope));
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_module(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::ModuleNode *)basic_node;
			
			gen<ModuleOp>(var, node->name, compile(node->scope));
			location(node->range);
		}
		
		void ByteCodeGenerator::convert_method(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::MethodNode *)basic_node;
			
			gen<MethodOp>(node->name, defer(node->scope));
			
			if(is_var(var))
				gen<LoadNilOp>(var);
		}
		
		void ByteCodeGenerator::convert_handler(Tree::Node *basic_node, var_t var)
		{
			auto node = (Tree::HandlerNode *)basic_node;
			
			scope->require_exceptions = true; // duh
			
			/*
			 * Allocate and setup the new exception block
			 */
			ExceptionBlock *exception_block = new ExceptionBlock;
			exception_block->parent = current_exception_block;
			
			/*
			 * Check for ensure node
			 */
			if(node->ensure_group)
				exception_block->ensure_label.label = create_label();
			else
				exception_block->ensure_label.label = 0;
			
			exception_blocks.push(exception_block);
			
			/*
			 * Use the new exception block
			 */
			current_exception_block = exception_block;
			gen<HandlerOp>(exception_block);
			
			/*
			 * Output the regular code
			 */
			to_bytecode(node->code, var);

			if(!node->rescues.empty())
			{
				/*
				 * Skip the rescue block
				 */
				Label *ok_label = create_label();
				gen<HandlerOp>(exception_block->parent);
				gen_branch(ok_label);
				
				/*
				 * Output rescue nodes. TODO: Check for duplicate nodes
				 */
				for(auto i = node->rescues.begin(); i != node->rescues.end(); ++i)
				{
					RuntimeExceptionHandler *handler = new RuntimeExceptionHandler;
					handler->type = RuntimeException;

					Label *handler_body = gen(create_label());

					handler->rescue_label.label = handler_body;

					exception_block->handlers.push(handler);
					
					//gen<FlushOp>(); Flush
					
					to_bytecode(i().group, var);
					
					gen_branch(ok_label);
				}
				
				gen(ok_label);
			}
			
			/*
			 * Restore the old exception frame
			 */
			current_exception_block = exception_block->parent;
			
			/*
			 * Check for ensure node
			 */
			if(node->ensure_group)
			{
				gen(exception_block->ensure_label.label);
				
				//gen<FlushOp>(); Flush
					
				/*
				 * Output ensure node
				 */
				to_bytecode(node->ensure_group, no_var);
				
				gen<UnwindOp>();
			}
		}
		
		void ByteCodeGenerator::finalize()
		{
			if(strings.size())
			{
				final->string_count = strings.size();

				final->strings = new const char_t *[final->string_count];

				for(size_t i = 0; i < final->string_count; ++i)
					final->strings[i] = strings[i];
			}
			
			size_t ranges = source_locs.size();
			
			if(ranges)
			{
				final->ranges = (Range *)std::malloc(ranges * sizeof(Range));

				ranges = 0;
				
				for(auto i = source_locs.begin(); i != source_locs.end(); ++i, ++ranges)
				{
					final->ranges[ranges] = *i().second;

					final->source_location.set(i().first, &final->ranges[ranges]);
				}
			}
			else
				final->ranges = nullptr;

			const char *opcodes = (const char *)opcode.compact<Prelude::Allocator::Standard>();

			for(auto i = branches.begin(); i != branches.end(); ++i)
			{
				BranchOp *op = (BranchOp *)&opcodes[(*i).first];

				op->pos = (*i).second->pos;
			}

			if(exception_blocks.size())
			{
				final->exception_blocks = new ExceptionBlock *[exception_blocks.size()];
				final->exception_block_count = exception_blocks.size();

				for(size_t i = 0; i < final->exception_block_count; ++i)
				{
					ExceptionBlock *block = exception_blocks[i];
					
					final->exception_blocks[i] = block;

					if(block->ensure_label.label)
						block->ensure_label.address = block->ensure_label.label->pos;
					else
						block->ensure_label.address = -1;

					for(auto i: block->handlers)
					{
						switch(i->type)
						{
							case RuntimeException:
							{
								RuntimeExceptionHandler *handler = (RuntimeExceptionHandler *)i;

								handler->rescue_label.address = handler->rescue_label.label->pos;

								break;
							}

							case ClassException:
							case FilterException:
							default:
								break;
						}
					}
				}
			}

			scope->children.clear();

			final->scope = nullptr;
			final->opcodes = opcodes;
			final->var_words = var_count;
			final->executor = &evaluate_block;
		}

		ByteCodeGenerator::ByteCodeGenerator(MemoryPool memory_pool, Tree::Scope *scope) :
			scope(scope),
			current_exception_block(0),
			exception_blocks(memory_pool),
			strings(memory_pool),
			opcode(memory_pool),
			branches(memory_pool),
			source_locs(memory_pool),
			self_var(no_var),
			heap_var(no_var),
			var_count(scope->variable_list.size()),
			memory_pool(memory_pool)
		{
			Value::assert_valid(scope);

			#ifdef MIRB_DEBUG_COMPILER
				label_count = 0;
			#endif
		}

		Block *ByteCodeGenerator::generate()
		{
			Value::assert_valid(scope);

			if(scope->final)
				final = scope->final;
			else
			{
				final = Collector::allocate_pinned<Mirb::Block>(scope->document);
				scope->final = final;
			}
			
			return_var = create_var();

			for(auto i = scope->zsupers.begin(); i != scope->zsupers.end(); ++i)
			{
				scope->require_args(*i);
			}

			body = create_label();
			epilog = create_label();

			if(scope->break_targets)
			{
				scope->require_exceptions = true;
				final->break_targets = new var_t[scope->break_targets];
			}
			else
				final->break_targets = 0;
			
			if(scope->heap_vars)
			{
				heap_var = create_var();
				
				gen<CreateHeapOp>(heap_var, scope->heap_vars);
			}

			if(scope->require_self)
			{
				self_var = create_var();

				gen<SelfOp>(self_var);
			}
			
			if(scope->block_parameter)
				gen<BlockOp>(ref(scope->block_parameter));
			
			size_t index = 0;

			for(auto i = scope->parameters.begin(); i != scope->parameters.end(); ++i)
			{
				if(i().type == Tree::Variable::Heap)
				{
					var_t value = create_var();

					gen<LoadArgOp>(value, index++);

					write_variable(*i, value);
				}
				else
					gen<LoadArgOp>(ref(*i), index++);
			}
			
			gen(body);

			to_bytecode(scope->group, return_var);

			gen(epilog);

			gen<ReturnOp>(return_var);

			#ifdef MIRB_DEBUG_COMPILER
				CodeGen::ByteCodePrinter printer(this);

				std::cout << printer.print() << std::endl;
			#endif
			
			finalize();

			return final;
		}
		
		Mirb::Block *ByteCodeGenerator::compile(Tree::Scope *scope)
		{
			Mirb::Block *result = Compiler::compile(scope, memory_pool);
			
			final->blocks.push(result);

			return result;
		}

		Mirb::Block *ByteCodeGenerator::defer(Tree::Scope *scope)
		{
			Mirb::Block *result = Compiler::defer(scope);

			final->blocks.push(result);

			return result;
		}

		bool ByteCodeGenerator::in_ensure_block()
		{
			ExceptionBlock *exception_block = current_exception_block;
			
			while(exception_block)
			{
				if(exception_block->ensure_label.label != 0)
					return true;

				exception_block = exception_block->parent;
			}
			
			return false;
		}
		
		Label *ByteCodeGenerator::create_label()
		{
			#ifdef MIRB_DEBUG_COMPILER
				return new (memory_pool) Label(label_count++);
			#else
				return new (memory_pool) Label;
			#endif
		}
		
		var_t ByteCodeGenerator::create_var()
		{
			return var_count++;
		}

		var_t ByteCodeGenerator::block_arg(Tree::Scope *scope, var_t break_dst)
		{
			var_t var = no_var;
			
			if(scope)
			{
				var = create_var();

				scope->break_dst = break_dst;

				auto *block_attach = defer(scope);
				
				VariableGroup group(this, scope->referenced_scopes.size());

				for(size_t i = 0; i < scope->referenced_scopes.size(); ++i)
				{
					if(scope->referenced_scopes[i] == this->scope)
						gen<MoveOp>(group[i], heap_var);
					else
						gen<LookupOp>(group[i], scope->referenced_scopes.index_of(scope->referenced_scopes[i]));
				}

				if(scope->break_id != Tree::Scope::no_break_id)
					final->break_targets[scope->break_id] = break_dst;

				gen<ClosureOp>(var, block_attach, group.size, group.use());
			}
			
			return var;
		}
		
		var_t ByteCodeGenerator::call_args(Tree::CountedNodeList &arguments, Tree::Scope *scope, size_t &argc, var_t &argv, var_t break_dst)
		{
			if(scope)
				Value::assert_valid(scope);

			if(!arguments.empty())
			{
				VariableGroup group(this, arguments.size);

				size_t param = 0;
				
				for(auto i = arguments.begin(); i != arguments.end(); ++i)
					to_bytecode(*i, group[param++]);

				argc = group.size;
				argv = group.use();
			}
			else
			{
				argc = 0;
				argv = no_var;
			}
			
			return block_arg(scope, break_dst);
		}
	};
};

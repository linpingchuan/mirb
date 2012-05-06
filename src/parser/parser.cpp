#include "parser.hpp"
#include "../compiler.hpp"
#include "../document.hpp"

namespace Mirb
{
	Parser::Parser(SymbolPool &symbol_pool, MemoryPool memory_pool, Document *document) :
		lexer(symbol_pool, memory_pool, *this),
		document(*document),
		memory_pool(memory_pool),
		void_list(nullptr),
		fragment(0),
		scope(0)
	{
	}
	
	Parser::~Parser()
	{
	}

	Range *Parser::capture()
	{
		return new (fragment) Range(lexer.lexeme);
	}
	
	void Parser::load()
	{
		mirb_debug_assert(document.data[document.length] == 0);

		lexer.load(document.data, document.length);
	}
	
	void Parser::report(const Range &range, std::string text, Message::Severity severity)
	{
		new (memory_pool) StringMessage(*this, range, severity, text);
	}

	bool Parser::is_constant(Symbol *symbol)
	{
		const char_t c = *symbol->string.raw();
		
		return c >= 'A' && c <= 'Z';
	}
	
	void Parser::error(std::string text)
	{
		report(lexer.lexeme.dup(memory_pool), text);
	}

	void Parser::expected(Lexeme::Type what, bool skip)
	{
		error("Expected " + Lexeme::describe_type(what) + ", but found " + lexer.lexeme.describe());
		
		if(skip)
			lexer.step();
	}
	
	void Parser::unexpected(bool skip)
	{
		error("Unexpected " + lexer.lexeme.describe());

		if(skip)
			lexer.step();
	}
	
	void Parser::parse_sep()
	{
		switch (lexeme())
		{
			case Lexeme::LINE:
			case Lexeme::SEMICOLON:
				lexer.step();
				break;

			default:
				expected(Lexeme::SEMICOLON);
				break;
		}
	}

	Range *Parser::parse_method_name(Symbol *&symbol)
	{
		Range *range = capture();

		switch(lexeme())
		{
			case Lexeme::IDENT:
			{
				symbol = lexer.lexeme.symbol;

				lexer.step();

				if(lexeme() == Lexeme::ASSIGN && lexer.lexeme.whitespace == false)
				{
					symbol = symbol_pool.get(symbol->string + "=");

					range->expand(lexer.lexeme);

					lexer.step();
				}

				return range;
			}
			
			case Lexeme::EXT_IDENT:
			{
				symbol = lexer.lexeme.symbol;

				lexer.step();

				return range;
			}
			
			case Lexeme::POWER:
			case Lexeme::MUL:
			case Lexeme::DIV:
			case Lexeme::MOD:
			case Lexeme::ADD:
			case Lexeme::SUB:
			case Lexeme::LEFT_SHIFT:
			case Lexeme::RIGHT_SHIFT:
			case Lexeme::AMPERSAND:
			case Lexeme::BITWISE_XOR:
			case Lexeme::BITWISE_OR:
			case Lexeme::COMPARE:
			case Lexeme::EQUALITY:
			case Lexeme::CASE_EQUALITY:
			case Lexeme::MATCHES:
			case Lexeme::NOT_MATCHES:
			case Lexeme::BITWISE_NOT:
			case Lexeme::LOGICAL_NOT:
			case Lexeme::UNARY_ADD:
			case Lexeme::UNARY_SUB:
			{
				symbol = symbol_pool.get(lexer.lexeme);

				lexer.step();

				return range;

			}

			case Lexeme::SQUARE_OPEN:
			{
				lexer.step();

				if(lexer.lexeme.whitespace == false && match(Lexeme::SQUARE_CLOSE))
				{
					if(lexer.lexeme.whitespace == false && lexeme() == Lexeme::ASSIGN)
					{
						symbol = symbol_pool.get("[]=");

						lexer.step();
					}
					else
					{
						symbol = symbol_pool.get("[]");
					}

					lexer.lexeme.prev_set(range);
				}
				else
					symbol = 0;

				return range;
			}
			
			default:
			{
				expected(Lexeme::IDENT);
					
				return 0;
			}
		}
	}
				
	Tree::Node *Parser::parse_variable(Symbol *symbol, Range *range)
	{
		if(is_constant(symbol))
		{
			return new (fragment) Tree::ConstantNode(nullptr, symbol, range);
		}
		else
		{
			auto result = new (fragment) Tree::VariableNode;
			
			result->var = scope->get_var<Tree::NamedVariable>(symbol);
			
			return result;
		}
	}
	
	void Parser::parse_arguments(Tree::CountedNodeList &arguments)
	{
		do
		{
			auto node = parse_operator_expression(false);
			
			if(node)
				arguments.append(node);
		}
		while(matches(Lexeme::COMMA));
	}
	
	bool Parser::is_assignment_op()
	{
		switch (lexeme())
		{
			case Lexeme::ASSIGN_POWER:
			case Lexeme::ASSIGN_ADD:
			case Lexeme::ASSIGN_SUB:
			case Lexeme::ASSIGN_MUL:
			case Lexeme::ASSIGN_DIV:
			case Lexeme::ASSIGN_MOD:
			case Lexeme::ASSIGN_LEFT_SHIFT:
			case Lexeme::ASSIGN_RIGHT_SHIFT:
			case Lexeme::ASSIGN_BITWISE_AND:
			case Lexeme::ASSIGN_BITWISE_XOR:
			case Lexeme::ASSIGN_BITWISE_OR:
			case Lexeme::ASSIGN_LOGICAL_AND:
			case Lexeme::ASSIGN_LOGICAL_OR:
			case Lexeme::ASSIGN:
				return true;
			
			default:
				return false;
		}
	}
	
	Tree::Node *Parser::parse_hash()
	{
		auto result = new (fragment) Tree::HashNode;
		
		lexer.step();
		
		if(lexeme() != Lexeme::CURLY_CLOSE)
		{
			do
			{
				auto range = capture();
				auto key = parse_operator_expression(false);
				lexer.lexeme.prev_set(range);

				if(lexeme() == Lexeme::COLON)
				{
					auto node = new (fragment) Tree::SymbolNode;
					auto call = static_cast<Tree::CallNode *>(key);

					if(key->type() == Tree::Node::Variable)
						node->symbol = static_cast<Tree::VariableNode *>(key)->var->name;
					else if(key->type() == Tree::Node::Call && call->can_be_var)
						node->symbol = call->method;
					else
						error("Use '=>' to associate non-identifier key");

					result->entries.append(node);

					lexer.step();
				}
				else
				{
					if(key)
						result->entries.append(key);

					match(Lexeme::ASSOC);
				}

				auto value = parse_operator_expression(false);
				
				if(value)
					result->entries.append(value);
			}
			while(matches(Lexeme::COMMA));
		}
		
		match(Lexeme::CURLY_CLOSE);
		
		return result;
	}

	Tree::Node *Parser::parse_array()
	{
		auto result = new (fragment) Tree::ArrayNode;
		
		lexer.step();
		
		if(lexeme() != Lexeme::SQUARE_CLOSE)
		{
			do
			{
				auto element = parse_operator_expression(false);
				
				if(element)
					result->entries.append(element);
			}
			while(matches(Lexeme::COMMA));
		}
		
		match(Lexeme::SQUARE_CLOSE);
		
		return result;
	}

	Tree::StringNode *Parser::parse_string(Value::Type type)
	{
		if(lexeme() == Lexeme::STRING)
		{
			auto data = lexer.lexeme.str;

			if(data->entries.size())
			{
				auto result = new (fragment) Tree::InterpolatedNode;

				result->result_type = type;

				process_string_entries(result, result->string);

				result->string = data->tail.copy<Tree::Fragment>(fragment);

				lexer.step();

				return result;
			}
			else
			{
				auto result = new (fragment) Tree::StringNode;
				
				result->result_type = type;

				result->string = data->tail.copy<Tree::Fragment>(fragment);
					
				lexer.step();

				return result;
			}
		}
		else
		{
			auto result = new (fragment) Tree::InterpolatedNode;
				
			result->result_type = type;

			do
			{
				auto pair = new (fragment) Tree::InterpolatedPairNode;

				process_string_entries(result, pair->string);

				lexer.step();
					
				pair->group = parse_group();
					
				result->pairs.append(pair);
			}
			while(lexeme() == Lexeme::STRING_CONTINUE);
				
			if(require(Lexeme::STRING_END))
			{
				process_string_entries(result, result->string);

				result->string = lexer.lexeme.str->tail.copy<Tree::Fragment>(fragment);

				lexer.step();
			}

			return result;
		}
	}

	void Parser::process_string_entries(Tree::InterpolatedNode *root, StringData::Entry &tail)
	{
		auto &entries = lexer.lexeme.str->entries;

		for(auto entry: entries)
		{
			auto pair = new (fragment) Tree::InterpolatedPairNode;
			pair->string = entry->copy<Tree::Fragment>(fragment);

			switch(entry->type)
			{
				case Lexeme::IVAR:
					pair->group = new (fragment) Tree::IVarNode(entry->symbol);
					break;

				case Lexeme::GLOBAL:
					pair->group = new (fragment) Tree::GlobalNode(entry->symbol);
					break;

				default:
					mirb_debug_abort("Unknown string type");
			}

			root->pairs.append(pair);
		}

		tail = lexer.lexeme.str->tail.copy<Tree::Fragment>(fragment);
	}

	Tree::Node *Parser::parse_factor()
	{
		switch (lexeme())
		{
			case Lexeme::KW_SPECIAL_FILE:
			{
				auto result = new (fragment) Tree::StringNode;

				result->result_type = Value::String;
				result->string.set<Tree::Fragment>(document.name.get_string(), fragment);

				lexer.step();

				return result;
			}

			case Lexeme::KW_BEGIN:
				return parse_begin();

			case Lexeme::KW_IF:
				return parse_if();

			case Lexeme::KW_UNLESS:
				return parse_unless();
				
			case Lexeme::KW_WHILE:
			case Lexeme::KW_UNTIL:
				return parse_loop();

			case Lexeme::KW_CASE:
				return parse_case();

			case Lexeme::KW_CLASS:
				return parse_class();

			case Lexeme::KW_MODULE:
				return parse_module();

			case Lexeme::KW_DEF:
				return parse_method();

			case Lexeme::KW_YIELD:
				return parse_yield();

			case Lexeme::KW_RETURN:
				return parse_return();

			case Lexeme::KW_BREAK:
				return parse_break();

			case Lexeme::KW_NEXT:
				return parse_next();

			case Lexeme::KW_REDO:
				return parse_redo();

			case Lexeme::KW_SUPER:
				return parse_super();

			case Lexeme::SQUARE_OPEN:
				return parse_array();
				
			case Lexeme::CURLY_OPEN:
				return parse_hash();
				
			case Lexeme::QUESTION:
			{
				Range range = lexer.lexeme;

				lexer.step();

				if(lexeme() == Lexeme::SCOPE && !lexer.lexeme.whitespace)
				{
					auto result = new (fragment) Tree::StringNode;
				
					result->result_type = Value::String;
					
					result->string.data = (const char_t *)":";
					result->string.length = 1;
					
					lexer.step();

					return result;
				}
				else
				{
					report(range, "Expected characater literal");
					return nullptr;
				}
			}
				
			case Lexeme::SYMBOL:
			{
				auto result = new (fragment) Tree::SymbolNode;

				result->symbol = lexer.lexeme.symbol;

				lexer.step();

				return result;
			}

			case Lexeme::COLON:
			{
				auto range = capture();

				lexer.step();

				if(lexeme() == Lexeme::STRING || lexeme() == Lexeme::STRING_START)
				{
					if(lexer.lexeme.whitespace)
						report(*range, "No whitespace between ':' and the string literal is allowed with symbol string literals");

					return parse_string(Value::Symbol);
				}
				else
				{
					report(*range, "Expected a symbol string literal, but found " + lexer.lexeme.describe());

					return 0;
				}
			}

			case Lexeme::STRING:
			case Lexeme::STRING_START:
				return parse_string(Value::String);

			case Lexeme::KW_SELF:
			{
				lexer.step();
				
				scope->require_self = true;

				return new (fragment) Tree::SelfNode;
			}

			case Lexeme::KW_TRUE:
			{
				lexer.step();

				return new (fragment) Tree::TrueNode;
			}

			case Lexeme::KW_FALSE:
			{
				lexer.step();

				return new (fragment) Tree::FalseNode;
			}

			case Lexeme::KW_NIL:
			{
				lexer.step();

				return new (fragment) Tree::NilNode;
			}

			case Lexeme::INTEGER:
			{
				auto result = new (fragment) Tree::IntegerNode;
					
				std::string str = lexer.lexeme.string();
					
				result->value = atoi(str.c_str());
					
				lexer.step();
					
				return result;
			}
			
			case Lexeme::SCOPE:
			{
				auto range = capture();

				lexer.step();

				if(require(Lexeme::IDENT))
				{
					range->expand(lexer.lexeme);

					Symbol *symbol = lexer.lexeme.symbol;

					if(is_constant(symbol))
					{
						Tree::Node *result = new (fragment) Tree::ConstantNode(nullptr, symbol, range, true);

						lexer.step();

						while(is_lookup())
						{
							Tree::Node *node = parse_lookup(result);

							result = node;
						}

						return result;
					}
					else
					{
						Tree::Node *call_node = parse_call(0, new (fragment) Tree::SelfNode, 0, false); // Try to parse it as a method or local variable

						if(call_node->type() == Tree::Node::Call)
							range->expand(*((Tree::CallNode *)call_node)->range);

						report(*range, "Can only reference constants when leaving out the lookup expression (Use ::Object to access non-constants)");

						return call_node;
					}
				}
			
				Symbol *symbol = lexer.lexeme.symbol;
					
				
				return new (fragment) Tree::IVarNode(symbol);
			}

			case Lexeme::IVAR:
			{
				Symbol *symbol = lexer.lexeme.symbol;
					
				lexer.step();
				
				return new (fragment) Tree::IVarNode(symbol);
			}

			case Lexeme::GLOBAL:
			{
				Symbol *symbol = lexer.lexeme.symbol;
					
				lexer.step();
				
				return new (fragment) Tree::GlobalNode(symbol);
			}

			case Lexeme::IDENT:
			{
				Symbol *symbol = lexer.lexeme.symbol;
				auto range = capture();

				lexer.step();
				
				scope->require_self = true; // TODO: Reduce this to self calls only

				return parse_call(symbol, new (fragment) Tree::SelfNode, range, true); // Function call, constant or local variable
			}
			
			case Lexeme::UNARY_ADD:
			case Lexeme::UNARY_SUB:
			case Lexeme::EXT_IDENT:
			{
				auto symbol = symbol_pool.get(lexer.lexeme);
				auto range = capture();

				lexer.step();
				
				scope->require_self = true;

				return parse_call(symbol, new (fragment) Tree::SelfNode, range, false);
			}

			case Lexeme::PARENT_OPEN:
			{
				lexer.step();

				Tree::Node *result = parse_group();

				match(Lexeme::PARENT_CLOSE);
					
				return result;
			}

			default:
			{
				error("Expected expression but found " + lexer.lexeme.describe());
				
				lexer.step();
					
				return 0;
			}
		}
	}
	
	Tree::Node *Parser::parse_power()
	{
		Tree::Node *result = parse_lookup_chain();
		
		while(lexeme() == Lexeme::POWER)
		{
			typecheck(result, [&](Tree::Node *result) -> Tree::Node * {
				auto node = new (fragment) Tree::BooleanOpNode;
			
				node->op = Lexeme::POWER;
				node->left = result;
			
				lexer.step();
			
				node->right = typecheck(parse_lookup_chain());

				return node;
			});
		}
		
		return result;
	}

	Tree::Node *Parser::parse_unary()
	{
		switch(lexeme())
		{
			case Lexeme::BITWISE_NOT:
			case Lexeme::LOGICAL_NOT:
			case Lexeme::ADD:
			case Lexeme::SUB:
			{
				auto result = new (fragment) Tree::UnaryOpNode;

				result->range = capture();
				result->op = (lexeme() == Lexeme::BITWISE_NOT || lexeme() == Lexeme::LOGICAL_NOT) ? lexeme() : Lexeme::operator_to_unary(lexeme());

				lexer.step();

				result->value = typecheck(parse_power());

				lexer.lexeme.prev_set(result->range);
				
				return result;
			}
			
			default:
				return parse_power();
		}
	}

	size_t Parser::operator_precedences[] = {
		8, // MUL,
		8, // DIV,
		8, // MOD,
		
		7, // ADD,
		7, // SUB,
		
		6, // LEFT_SHIFT,
		6, // RIGHT_SHIFT,
		
		5, // AMPERSAND, // BITWISE_AND
		
		4, // BITWISE_XOR,
		4, // BITWISE_OR,
		
		1, // LOGICAL_AND,
		
		0, // LOGICAL_OR,
		
		3, // GREATER,
		3, // GREATER_OR_EQUAL,
		3, // LESS,
		3, // LESS_OR_EQUAL,
		
		2, // COMPARE,
		2, // EQUALITY,
		2, // CASE_EQUALITY,
		2, // NO_EQUALITY,
		2, // MATCHES,
		2, // NOT_MATCHES,
	};

	bool Parser::is_precedence_operator(Lexeme::Type op)
	{
		return op >= Lexeme::precedence_operators_start && op <= Lexeme::precedence_operators_end;
	}

	Tree::Node *Parser::parse_precedence_operator()
	{
		return parse_precedence_operator(parse_unary(), 0);
	}

	Tree::Node *Parser::parse_precedence_operator(Tree::Node *left, size_t min_precedence)
	{
		while(true)
		{
			Lexeme::Type op = lexeme();
			
			if(!is_precedence_operator(op))
				break;

			size_t precedence = operator_precedences[op - Lexeme::precedence_operators_start];
			
			if(precedence < min_precedence)
				break;
			
			typecheck(left, [&](Tree::Node *left) -> Tree::Node * {
				Range *range = capture();
			
				lexer.step();

				Tree::Node *right = typecheck(parse_unary());

				while(true)
				{
					Lexeme::Type next_op = lexeme();

					if(!is_precedence_operator(next_op))
						break;

					size_t next_precedence = operator_precedences[next_op - Lexeme::precedence_operators_start];

					if(next_precedence <= precedence)
						break;

					right = typecheck(parse_precedence_operator(right, next_precedence));
				}
			
				auto node = new (fragment) Tree::BinaryOpNode;
			
				node->op = op;
				node->left = left;
				node->right = right;
				node->range = range;

				return node;
			});
		}

		return left;
	}

	Tree::Node *Parser::typecheck(Tree::Node *result)
	{
		if(result->single())
			return result;

		Tree::MultipleExpressionsNode *node = (Tree::MultipleExpressionsNode *)result;

		report(*node->range, "Unexpected multiple expressions");

		return nullptr;
	}
	
	Tree::Node *Parser::parse_splat_expression()
	{
		if(matches(Lexeme::MUL))
		{
			if(matches(Lexeme::MUL))
			{
				Range range = lexer.lexeme;

				while(matches(Lexeme::MUL));

				lexer.lexeme.prev_set(&range);

				report(range, "Nested splat operators are not allowed");
			}

			auto result = new (fragment) Tree::SplatNode;

			result->expression = typecheck(parse_factor());

			return result;
		}
		else
			return parse_ternary_if();
	}

	void Parser::process_lhs(Tree::Node *&lhs, const Range &range)
	{
		if(!lhs)
			return;

		switch(lhs->type())
		{
			case Tree::Node::Group:
			{
				auto group = static_cast<Tree::GroupNode *>(lhs);
				
				if(group->statements.first && (group->statements.first == group->statements.last))
				{
					auto node = new (fragment) Tree::MultipleExpressionsNode;
					node->expressions.append(new (fragment) Tree::MultipleExpressionNode(group->statements.first, range));

					process_multiple_lhs(node);
					
					lhs = node;

					return;
				}

				break;
			}

			case Tree::Node::MultipleExpressions:
				process_multiple_lhs(static_cast<Tree::MultipleExpressionsNode *>(lhs));
				return;
				
			case Tree::Node::IVar:
			case Tree::Node::Global:
			case Tree::Node::Constant:
			case Tree::Node::Variable:
				return;

			case Tree::Node::Call:
			{
				auto node = static_cast<Tree::CallNode *>(lhs);

				if(node->can_be_var)
				{
					lhs = parse_variable(node->method, node->range);
					
					return;
				}
				else if(node->subscript || (node->arguments.empty() && !node->block))
				{
					Symbol *mutated = node->method;
					
					if(mutated)
						mutated = Symbol::from_char_array(node->method->string + "=");

					node->method = mutated;

					return;
				}

				break;
			}

			case Tree::Node::Splat:
			{
				auto node = static_cast<Tree::SplatNode *>(lhs);

				process_lhs(node->expression, range);

				return;
			}

			default:
				break;
		}

		report(range, "Not an assignable expression");
	}
	
	void Parser::process_multiple_lhs(Tree::MultipleExpressionsNode *node)
	{
		bool seen_splat = false;
		int size = 0;
		int index = 0;
		int offset = 1;
		int splat_index = -1;

		for(auto expr: node->expressions)
		{
			expr->size = size;
			expr->index = index;

			if(expr->expression->type() == Tree::Node::Splat)
			{
				splat_index = size;
				index = -1;
				offset = -1;

				if(seen_splat)
					report(expr->range, "You can only have one splat operator on the left-hand side");
				else
					seen_splat = true;
			}
			else
			{
				size += 1;
				index += offset;
			}

			process_lhs(expr->expression, expr->range);
		}
		
		node->expression_count = size;
		node->splat_index = splat_index;
	}

	Tree::Node *Parser::parse_multiple_expressions(bool allow_multiples)
	{
		Range range = lexer.lexeme;
		auto result = parse_splat_expression();

		if(allow_multiples)
		{
			Tree::MultipleExpressionsNode *node;

			bool seen_splat = result && (result->type() == Tree::Node::Splat);
			bool is_multi = lexeme() == Lexeme::COMMA || seen_splat;

			if(is_multi)
			{
				lexer.lexeme.prev_set(&range);

				node = new (fragment) Tree::MultipleExpressionsNode;
				node->expressions.append(new (fragment) Tree::MultipleExpressionNode(result,  range));
			}
			else
				return result;

			if(matches(Lexeme::COMMA))
			{
				do
				{
					Range result_range = lexer.lexeme;
					result = typecheck(parse_splat_expression());
					lexer.lexeme.prev_set(&result_range);

					if(result)
						node->expressions.append(new (fragment) Tree::MultipleExpressionNode(result,  result_range));

				} while (matches(Lexeme::COMMA));

			}
			
			lexer.lexeme.prev_set(&range);
			node->range = new (fragment) Range(range);

			return node;
		}
		else
			return result;
	}

	Tree::Node *Parser::build_assignment(Tree::Node *left, bool allow_multiples)
	{
		if(lexeme() == Lexeme::ASSIGN)
		{
			auto result = new (fragment) Tree::AssignmentNode;
			
			lexer.step();
			
			result->op = Lexeme::ASSIGN;
			
			result->left = left;
			
			result->right = parse_assignment(allow_multiples);
			
			return result;
		}
		else
		{
			auto result = new (fragment) Tree::AssignmentNode;
			auto binary_op = new (fragment) Tree::BinaryOpNode;

			result->op = Lexeme::ASSIGN;
			
			result->left = left;
			result->right = binary_op;
			binary_op->left = left;
			binary_op->range = capture();
			binary_op->op = Lexeme::assign_to_operator(lexeme());
			
			lexer.step();
			
			binary_op->right = parse_assignment(allow_multiples);
			
			return result;
		}
	};

	Tree::Node *Parser::parse_assignment(bool allow_multiples)
	{
		Tree::Node *result = parse_multiple_expressions(allow_multiples);
		
		if(is_assignment_op())
			return process_assignment(result, allow_multiples);
		else
			return result;
	}

	Tree::Node *Parser::process_assignment(Tree::Node *input, bool allow_multiples)
	{
		if(!input)
		{
			lexer.step();
			return parse_expression();
		}

		switch(input->type())
		{
			case Tree::Node::MultipleExpressions:
			{
				auto node = (Tree::MultipleExpressionsNode *)input;

				if(lexeme() != Lexeme::ASSIGN)
					error("Can only use regular assign with multiple expression assignment");

				lexer.step();

				process_multiple_lhs(node);

				auto result = new (fragment) Tree::AssignmentNode;
			
				result->op = Lexeme::ASSIGN;
			
				result->left = node;
			
				result->right = parse_assignment(allow_multiples);

				return result;
			}
			break;

			case Tree::Node::Call:
			{
				auto node = (Tree::CallNode *)input;
					
				if(!node->subscript)
					if(!node->arguments.empty() || node->block)
						break;
				
				if(node->can_be_var)
					return build_assignment(parse_variable(node->method, node->range), allow_multiples);
				else
				{
					Symbol *mutated = node->method;
					
					if(mutated)
						mutated = Symbol::from_char_array(node->method->string + "=");

					if(lexeme() == Lexeme::ASSIGN)
					{
						lexer.step();
						
						node->method = mutated;
						
						Tree::Node *argument = parse_assignment(allow_multiples);

						if(argument)
							node->arguments.append(argument);

						if(node->range)
							lexer.lexeme.prev_set(node->range);

						return input;
					}
					else
					{
						Range assign_range = lexer.lexeme;
					
						// TODO: Save node->object in a temporary variable
						auto result = new (fragment) Tree::CallNode;
						
						result->object = node->object;
						result->method = mutated;
						result->block = 0;

						if(node->range)
							result->range = capture();
						else
							result->range = 0;
						
						auto binary_op = new (fragment) Tree::BinaryOpNode;
						
						binary_op->left = node;
						binary_op->range = capture();
						binary_op->op = Lexeme::assign_to_operator(lexeme());
						
						lexer.step();
						
						binary_op->right = parse_assignment(allow_multiples);

						if(binary_op->right->type() == Tree::Node::MultipleExpressions)
							report(assign_range, "Can only use regular assign with multiple expression assignment");

						result->arguments.append(binary_op);

						lexer.lexeme.prev_set(result->range);
						
						return result;
					}
				}

				break;
			};
			
			case Tree::Node::Global:
			case Tree::Node::Variable:
			case Tree::Node::Constant: //TODO: Make sure constant's object is not re-evaluated
			case Tree::Node::IVar:
				return build_assignment(input, allow_multiples);

			default:
				break;
		}
			
		error("Cannot assign a value to an expression.");
		
		lexer.step();

		parse_expression();

		return input;
	}
	
	Tree::Node *Parser::parse_boolean_unary()
	{
		if(lexeme() == Lexeme::KW_NOT)
		{
			lexer.step();

			auto result = parse_assignment(true);

			typecheck(result, [&](Tree::Node *result) {
				return new (fragment) Tree::BooleanNotNode(result);
			});

			return result;
		}
		else
		{
			return parse_assignment(true);
		}
	}

	Tree::Node *Parser::parse_boolean()
	{
		Tree::Node *result = parse_boolean_unary();
		
		while(lexeme() == Lexeme::KW_AND || lexeme() == Lexeme::KW_OR)
		{
			typecheck(result, [&](Tree::Node *result) -> Tree::Node * {
				auto node = new (fragment) Tree::BooleanOpNode;
				
				node->op = lexeme();
				node->left = result;
			
				lexer.step();
			
				node->right = parse_boolean_unary();

				return node;
			});
		}
		
		return result;
	}
	
	Tree::Node *Parser::parse_statements()
	{
		skip_lines();
		
		auto group = new (fragment) Tree::GroupNode;
		
		if(!is_sep())
		{
			if(!is_expression())
				return group;

			Tree::Node *node = parse_statement();

			if(node && node->type() == Tree::Node::MultipleExpressions)
			{
				Range range = lexer.lexeme;

				bool error = is_sep();
				
				skip_seps();
		
				while(is_expression())
				{
					error = true;
					typecheck(parse_statement());
			
					if (!is_sep())
						break;
			
					skip_seps();
				}

				lexer.lexeme.prev_set(&range);

				if(error)
					report(range, "Unexpected statement(s) after multiple expression(s)");

				return node;
			}
			
			if(node)
				group->statements.append(node);
		}
		
		skip_seps();
		
		while(is_expression())
		{
			Tree::Node *node = typecheck(parse_statement());
			
			if(node)
				group->statements.append(node);
			
			if (!is_sep())
				break;
			
			skip_seps();
		}

		return group;
	}
	
	bool Parser::is_sep()
	{
		return lexeme() == Lexeme::SEMICOLON || lexeme() == Lexeme::LINE;
	}
	
	void Parser::skip_seps()
	{
		while(is_sep())
			lexer.step();
	}
	
	Tree::Scope *Parser::parse_main()
	{
		fragment = *new Tree::Fragment::Base(Tree::Chunk::main_size);
		
		Tree::Scope *result;
		
		allocate_scope(Tree::Scope::Top, [&] {
			result = scope;
			scope->owner = scope;
			scope->group = parse_group();
		});
		
		match(Lexeme::END);
		
		return result;
	}
};

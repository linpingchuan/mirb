#pragma once
#include "../common.hpp"
#include <Prelude/List.hpp>
#include "../lexer/lexeme.hpp"
#include "../symbol-pool.hpp"
#include "node.hpp"

namespace Mirb
{
	class Parser;

	namespace CodeGen
	{
		class Label;
	};
	
	namespace Tree
	{
		class Scope;
		class Variable;
		class NamedVariable;
		
		struct GroupNode:
			public Node
		{
			NodeType type() { return Group; }
			
			NodeList statements;
		};
		
		struct SplatNode:
			public Node
		{
			NodeType type() { return Splat; }
			
			Node *expression;
		};
		
		struct MultipleExpressionNode
		{
			ListEntry<MultipleExpressionNode> entry;
			Node *expression;
			Range range;
			int index;
			size_t size;

			MultipleExpressionNode(Node *expression, const Range &range) : expression(expression), range(range) {}
		};
		
		struct MultipleExpressionsNode:
			public Node
		{
			NodeType type() { return MultipleExpressions; }
			
			Range *range;
			List<MultipleExpressionNode> expressions;
			Range *extended;
			size_t splat_index;
			size_t expression_count;

			MultipleExpressionsNode() : extended(nullptr) {}
		};
		
		struct BinaryOpNode:
			public Node
		{
			NodeType type() { return BinaryOp; }
			
			Node *left;
			Node *right;
			Lexeme::Type op;
			
			Range *range;
		};

		struct VariableNode;
		
		struct AssignmentNode:
			public BinaryOpNode
		{
			NodeType type() { return Assignment; }
		};
		
		struct BooleanOpNode:
			public BinaryOpNode
		{
			NodeType type() { return BooleanOp; }
		};
		
		struct UnaryOpNode:
			public Node
		{
			NodeType type() { return UnaryOp; }
			
			UnaryOpNode() {};
			UnaryOpNode(Lexeme::Type op, Node *value, Range *range) : op(op), value(value), range(range) {}
			
			Lexeme::Type op;
			Node *value;
			
			Range *range;
		};
		
		struct BooleanNotNode:
			public UnaryOpNode
		{
			NodeType type() { return BooleanNot; }
			
			BooleanNotNode(Node *value) : UnaryOpNode(Lexeme::LOGICAL_NOT, value, nullptr) {}
		};
		
		struct DataNode:
			public Node
		{
			NodeType type() { return Data; }
			
			Value::Type result_type;
			
			InterpolateData::Entry data;
		};
		
		struct InterpolatePairNode
		{
			ListEntry<InterpolatePairNode> entry;
			InterpolateData::Entry string;

			Node *group;
		};
		
		struct InterpolateNode:
			public DataNode
		{
			NodeType type() { return Interpolate; }
			
			List<InterpolatePairNode> pairs;
		};
		
		struct IntegerNode:
			public Node
		{
			NodeType type() { return Integer; }
			
			int value;
		};

		struct FloatNode:
			public Node
		{
			NodeType type() { return Float; }
			
			double value;
		};

		struct VariableNode:
			public Node
		{
			NodeType type() { return Variable; }
			
			Tree::NamedVariable *var; // Might contain an unnamed variable if it's the implict block parameter. TODO: Make an own yield node to fix this.
			
			VariableNode() {}
			VariableNode(Tree::NamedVariable *var) : var(var) {}
		};
		
		struct IVarNode:
			public Node
		{
			NodeType type() { return IVar; }
			
			Mirb::Symbol *name;
			
			IVarNode(Mirb::Symbol *name) : name(name) {}
		};
		
		struct GlobalNode:
			public Node
		{
			NodeType type() { return Global; }
			
			Mirb::Symbol *name;
			
			GlobalNode(Mirb::Symbol *name) : name(name) {}
		};
		
		struct ConstantNode:
			public Node
		{
			NodeType type() { return Constant; }

			bool top_scope;
	
			Node *obj;
			Mirb::Symbol *name;

			Range *range;
			
			ConstantNode() {}
			ConstantNode(Node *obj, Mirb::Symbol *name, Range *range, bool top_scope = false) : top_scope(top_scope), obj(obj), name(name), range(range) {}
		};

		struct SelfNode:
			public Node
		{
			NodeType type() { return Self; }
		};

		struct NilNode:
			public Node
		{
			NodeType type() { return Nil; }
		};

		struct TrueNode:
			public Node
		{
			NodeType type() { return True; }
		};
		
		struct FalseNode:
			public Node
		{
			NodeType type() { return False; }
		};
		
		struct SymbolNode:
			public Node
		{
			NodeType type() { return Symbol; }
			
			Mirb::Symbol *symbol;
		};
		
		struct RangeNode:
			public LocationNode
		{
			NodeType type() { return NodeRange; }

			bool inclusive;
			Node *left;
			Node *right;
		};
		
		struct ArrayNode:
			public Node
		{
			NodeType type() { return Array; }
			
			CountedNodeList entries;
		};
		
		struct HashNode:
			public Node
		{
			NodeType type() { return Hash; }
			
			CountedNodeList entries;
		};
		
		struct BlockNode:
			public SimpleNode
		{
			NodeType type() { return Block; }
			
			Scope *scope;
		};
		
		struct BlockArg
		{
			Range range;
			Node *node;

			BlockArg(const Range &range, Node *node) : range(range), node(node) {}
		};
		
		struct InvokeNode:
			public Node
		{
			NodeType type() { return Invoke; }
			
			bool variadic;
			CountedNodeList arguments;
			BlockNode *block; // can be zero
			BlockArg *block_arg;

			Range *range;

			InvokeNode() : variadic(false), block_arg(nullptr) {}
		};
		
		struct CallNode:
			public InvokeNode
		{
			NodeType type() { return Call; }
			
			bool can_be_var;
			bool subscript;

			Node *object;
			Mirb::Symbol *method;

			CallNode() : can_be_var(false), subscript(false) {}
		};
		
		struct SuperNode:
			public InvokeNode
		{
			NodeType type() { return Super; }
			
			bool pass_args;
		};
		
		struct IfNode:
			public Node
		{
			NodeType type() { return If; }
			
			bool inverted;
			
			Node *left;
			Node *middle;
			Node *right;
		};
		
		struct CaseEntry
		{
			ListEntry<CaseEntry> entry;
			Range range;
			Node *pattern;
			Node *group;
		};
		
		struct CaseNode:
			public Node
		{
			NodeType type() { return Case; }
			
			Node *value;

			List<CaseEntry> clauses;
			Node *else_clause;
		};

		struct HandlerNode;
		
		struct LoopNode:
			public Node
		{
			NodeType type() { return Loop; }
			
			bool inverted;
			
			Node *body;
			Node *condition;
			HandlerNode *handler;
			CodeGen::Label *label_start;
			CodeGen::Label *label_body;
			CodeGen::Label *label_end;

			LoopNode() : handler(nullptr) {}
		};
		
		struct RescueNode:
			public ListNode
		{
			NodeType type() { return Rescue; }
			
			Node *pattern;
			Node *var;
			Node *group;
		};
		
		typedef List<RescueNode, ListNode> RescueList;
		
		struct HandlerNode:
			public Node
		{
			NodeType type() { return Handler; }
			
			Node *code;
			RescueList rescues;
			Node *else_group;
			Node *ensure_group;
			LoopNode *loop;

			HandlerNode() : else_group(nullptr), ensure_group(nullptr), loop(nullptr) {}
		};
		
		//TODO: Make sure no void nodes are in found in expressions.
		struct VoidNode:
			public Node
		{
			NodeType type() { return Void; }
			
			Range *range;
			bool in_ensure;
			LoopNode *target;
			
			ListEntry<VoidNode> void_entry;

			VoidNode(Range *range) : range(range), in_ensure(false), target(nullptr) {}
		};
		
		struct ReturnNode:
			public VoidNode
		{
			NodeType type() { return Return; }
			
			ReturnNode(Range *range, Node *value) : VoidNode(range), value(value) {}
			
			Node *value;
		};
		
		struct NextNode:
			public VoidNode
		{
			NodeType type() { return Next; }
			
			NextNode(Range *range, Node *value) : VoidNode(range), value(value) {}
			
			Node *value;
		};
		
		struct BreakNode:
			public VoidNode
		{
			NodeType type() { return Break; }
			
			BreakNode(Range *range, Node *value) : VoidNode(range), value(value) {}
			
			Node *value;
		};
		
		struct RedoNode:
			public VoidNode
		{
			NodeType type() { return Redo; }
			
			RedoNode(Range *range) : VoidNode(range) {}
		};
		
		struct ModuleNode:
			public Node
		{
			NodeType type() { return Module; }
			
			bool top_scope;
	
			Node *scoped;
			Mirb::Symbol *name;
			Scope *scope;

			Range *range;
		};
		
		struct ClassNode:
			public ModuleNode
		{
			NodeType type() { return Class; }
			
			Node *super;
		};
		
		struct MethodNode:
			public Node
		{
			NodeType type() { return Method; }
			
			Node *singleton;
			Mirb::Symbol *name;
			Scope *scope;
			Range *range;
		};
		
		struct AliasNode:
			public LocationNode
		{
			NodeType type() { return Alias; }
			
			Node *new_name;
			Node *old_name;
		};
	};
};

#pragma once
#include <vector>
#include <optional>
#include <variant>

#include "fe/data/types.h"
#include "fe/data/ast_data.h"
#include "fe/data/constants_store.h"
#include "utils/memory/data_store.h"

namespace fe::core_ast
{
	enum class node_type
	{
		NOP,

		// Values
		NUMBER,
		STRING,
		BOOLEAN,
		TUPLE,
		SUM,
		REFERENCE,

		// Stack manipulation
		 POP,
		 PUSH,
		 STACK_ALLOC,
		 STACK_DEALLOC,

		// Stack manipulation descriptors
		PARAM,
		DYNAMIC_PARAM,
		VARIABLE,
		DYNAMIC_VARIABLE,
		STATIC_OFFSET,
		RELATIVE_OFFSET,
		STACK_LABEL,

		// Functions and scopes
		FUNCTION,
		FUNCTION_CALL,
		RET,
		BLOCK,

		// Control flow
		LABEL,
		JMP, JNZ, JZ,

		// Logic ops
		LT, GT, LEQ, GEQ, EQ, NEQ, AND, OR, NOT,

		// Arithmetic ops
		ADD, SUB, MUL, DIV, MOD, NEG
	};

	constexpr bool is_binary_op(node_type kind)
	{
		return (kind == node_type::LT
			|| kind == node_type::GT
			|| kind == node_type::LEQ
			|| kind == node_type::GEQ
			|| kind == node_type::EQ
			|| kind == node_type::NEQ
			|| kind == node_type::AND
			|| kind == node_type::OR
			|| kind == node_type::ADD
			|| kind == node_type::SUB
			|| kind == node_type::MUL
			|| kind == node_type::DIV
			|| kind == node_type::MOD);
	}

	constexpr bool is_unary_op(node_type kind)
	{
		return (kind == node_type::NOT);
	}

	struct node
	{
		node() : kind(node_type::NOP), id(0) {}
		node(node_type t) : kind(t) {}

		node_type kind;
		node_id id;
		std::vector<node_id> children;
		std::optional<node_id> parent_id;
		std::optional<size_t> size;
		std::optional<data_index> data_index;
		std::optional<scope_index> value_scope_id;
	};

	class ast
	{
		friend class ast_helper;

		memory::dynamic_store<node> nodes;
		memory::dynamic_store<function_data> function_data_store;
		memory::dynamic_store<function_call_data> function_call_data_store;
		memory::dynamic_store<label> label_store;
		memory::dynamic_store<relative_offset> relative_offset_store;
		memory::dynamic_store<stack_label> stack_label_store;
		memory::dynamic_store<size> size_store;
		memory::dynamic_store<var_data> var_store;
		memory::dynamic_store<return_data> return_data_store;
		constants_store constants;

		node_id root;

	public:
		ast(node_type t);
		node_id root_id();

		// Nodes
		node_id create_node(node_type t);
		node_id create_node(node_type t, node_id parent);
		node& parent_of(node_id id);
		std::vector<node_id>& children_of(node_id id);
		void link_child_parent(node_id child, node_id parent);
		node& get_node(node_id id);

		// Node data 
		template<class DataType>
		DataType& get_data(data_index i);
		template<class DataType>
		DataType& get_node_data(node& i) { return get_data<DataType>(*i.data_index); }
		template<class DataType>
		DataType& get_node_data(node_id i) { return get_data<DataType>(*get_node(i).data_index); }
		template<> boolean& get_data<boolean>(data_index i) { return constants.get<boolean>(i); }
		template<> string& get_data<string>(data_index i) { return constants.get<string>(i); }
		template<> number& get_data<number>(data_index i) { return constants.get<number>(i); }
		template<> function_data& get_data<function_data>(data_index i) { return function_data_store.get_at(i); }
		template<> function_call_data& get_data<function_call_data>(data_index i) { return function_call_data_store.get_at(i); }
		template<> label& get_data<label>(data_index i) { return label_store.get_at(i); }
		template<> relative_offset& get_data<relative_offset>(data_index i) { return relative_offset_store.get_at(i); }
		template<> stack_label& get_data<stack_label>(data_index i) { return stack_label_store.get_at(i); }
		template<> size& get_data<size>(data_index i) { return size_store.get_at(i); }
		template<> var_data& get_data<var_data>(data_index i) { return var_store.get_at(i); }
		template<> return_data& get_data<return_data>(data_index i) { return return_data_store.get_at(i); }

	private:
		std::optional<data_index> create_node_data(node_type t);
	};

	class ast_helper
	{
		ast& a;

	public:
		ast_helper(ast& a) : a(a) {}

		template<typename F>
		void for_all_t(core_ast::node_type t, F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i) && a.nodes.get_at(i).kind == t)
				{
					f(a.nodes.get_at(i));
				}
			}
		}

		template<typename F>
		void for_all(F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i))
				{
					f(a.nodes.get_at(i));
				}
			}
		}

		template<typename F>
		std::optional<node_id> find_if(F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i) && a.nodes.get_at(i).kind == t)
				{
					if (f(a.nodes.get_at(i))) return a.nodes.get_at(i).id;
				}
			}
			return std::nullopt;
		}


	};
}

#pragma once
#include <optional>
#include <variant>
#include <vector>

#include "fe/data/ast_data.h"
#include "fe/data/constants_store.h"
#include "fe/data/types.h"
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
		JMP,
		JNZ,
		JZ,

		// Logic ops
		LT,
		GT,
		LEQ,
		GEQ,
		EQ,
		NEQ,
		AND,
		OR,
		NOT,

		// Arithmetic ops
		ADD,
		SUB,
		MUL,
		DIV,
		MOD,
		NEG
	};

	constexpr bool is_binary_op(node_type kind)
	{
		return (kind == node_type::LT || kind == node_type::GT || kind == node_type::LEQ ||
			kind == node_type::GEQ || kind == node_type::EQ || kind == node_type::NEQ ||
			kind == node_type::AND || kind == node_type::OR || kind == node_type::ADD ||
			kind == node_type::SUB || kind == node_type::MUL ||
			kind == node_type::DIV || kind == node_type::MOD);
	}

	constexpr bool is_unary_op(node_type kind) { return (kind == node_type::NOT); }

	struct node
	{
		node() : kind(node_type::NOP), id(0) {}
		node(node_type t) : kind(t) {}

		node_type kind;
		node_id id;
		std::vector<node_id> children;
		std::optional<node_id> parent_id;
		std::optional<size_t> size;
		std::optional<data_index_t> data_index;
		std::optional<scope_index> value_scope_id;
	};

	class ast;

	namespace detail
	{
		template <class DataType> DataType &get_data(ast &a, data_index_t i);
	}

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
		node &parent_of(node_id id);
		std::vector<node_id> &children_of(node_id id);
		void link_child_parent(node_id child, node_id parent);
		node &get_node(node_id id);

		// Node data
		template <class DataType> DataType &get_data(data_index_t i)
		{
			return detail::get_data<DataType>(*this, i);
		}
		template <class DataType> friend DataType &detail::get_data(ast &, data_index_t i);
		template <class DataType> DataType &get_node_data(node &i)
		{
			return get_data<DataType>(*i.data_index);
		}
		template <class DataType> DataType &get_node_data(node_id i)
		{
			return get_data<DataType>(*get_node(i).data_index);
		}

	      private:
		std::optional<data_index_t> create_node_data(node_type t);
	};

	namespace detail
	{
		template <> boolean &get_data<boolean>(ast &a, data_index_t i);
		template <> string &get_data<string>(ast &a, data_index_t i);
		template <> number &get_data<number>(ast &a, data_index_t i);
		template <> function_data &get_data<function_data>(ast &a, data_index_t i);
		template <>
		function_call_data &get_data<function_call_data>(ast &a, data_index_t i);
		template <> label &get_data<label>(ast &a, data_index_t i);
		template <> relative_offset &get_data<relative_offset>(ast &a, data_index_t i);
		template <> stack_label &get_data<stack_label>(ast &a, data_index_t i);
		template <> size &get_data<size>(ast &a, data_index_t i);
		template <> var_data &get_data<var_data>(ast &a, data_index_t i);
		template <> return_data &get_data<return_data>(ast &a, data_index_t i);
	} // namespace detail

	class ast_helper
	{
		ast &a;

	      public:
		ast_helper(ast &a) : a(a) {}

		template <typename F> void for_all_t(core_ast::node_type t, F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i) && a.nodes.get_at(i).kind == t)
				{ f(a.nodes.get_at(i)); } }
		}

		template <typename F> void for_all(F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i)) { f(a.nodes.get_at(i)); }
			}
		}

		template <typename F> std::optional<node_id> find_if(F f)
		{
			for (auto i = 0; i < a.nodes.size(); i++)
			{
				if (a.nodes.is_occupied(i))
				{
					if (f(a.nodes.get_at(i))) return a.nodes.get_at(i).id;
				}
			}
			return std::nullopt;
		}
	};
} // namespace fe::core_ast

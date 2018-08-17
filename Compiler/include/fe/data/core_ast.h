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

		NUMBER,
		STRING,
		BOOLEAN,
		TUPLE,
		ARRAY,

		SALLOC,
		SDEALLOC,
		MOVE,

		FUNCTION,
		FUNCTION_CALL,
		RET,

		BLOCK,
		LABEL,
		JMP, JNZ,
		REFERENCE,

		// logic ops
		LT, GT, LEQ, GEQ, EQ, NEQ, AND, OR,
		// arithmetic ops
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
		memory::dynamic_store<node> nodes;
		memory::dynamic_store<move_data> move_data_store;
		memory::dynamic_store<function_data> function_data_store;
		memory::dynamic_store<function_call_data> function_call_data_store;
		memory::dynamic_store<label> label_store;
		memory::dynamic_store<size> size_store;
		memory::dynamic_store<return_data> return_data_store;
		constants_store constants;

		node_id root;

	public:
		ast(node_type t)
		{
			root = nodes.create();
			nodes.get_at(root) = node(t);
			nodes.get_at(root).id = root;
			nodes.get_at(root).data_index = create_node_data(t);
		}

		node_id root_id()
		{
			return root;
		}

		// Nodes
		node_id create_node(node_type t)
		{
			auto new_node = nodes.create();
			get_node(new_node).id = new_node;
			get_node(new_node).kind = t;
			get_node(new_node).data_index = create_node_data(t);
			return new_node;
		}

		node_id create_node(node_type t, node_id parent)
		{
			auto new_node = nodes.create();
			get_node(new_node).id = new_node;
			get_node(new_node).kind = t;
			get_node(new_node).data_index = create_node_data(t);
			get_node(new_node).parent_id = parent;
			get_node(parent).children.push_back(new_node);
			return new_node;
		}

		node& parent_of(node_id id)
		{
			return nodes.get_at(*nodes.get_at(id).parent_id);
		}

		std::vector<node_id>& children_of(node_id id)
		{
			return nodes.get_at(id).children;
		}

		void link_child_parent(node_id child, node_id parent)
		{
			children_of(parent).push_back(child);
			get_node(child).parent_id = parent;
		}

		node& get_node(node_id id)
		{
			return nodes.get_at(id);
		}

		// Node data 
		template<class DataType>
		DataType& get_data(data_index i);
		template<> boolean& get_data<boolean>(data_index i) { return constants.get<boolean>(i); }
		template<> string& get_data<string>(data_index i) { return constants.get<string>(i); }
		template<> number& get_data<number>(data_index i) { return constants.get<number>(i); }
		template<> move_data& get_data<move_data>(data_index i) { return move_data_store.get_at(i); }
		template<> function_data& get_data<function_data>(data_index i) { return function_data_store.get_at(i); }
		template<> function_call_data& get_data<function_call_data>(data_index i) { return function_call_data_store.get_at(i); }
		template<> label& get_data<label>(data_index i) { return label_store.get_at(i); }
		template<> size& get_data<size>(data_index i) { return size_store.get_at(i); }
		template<> return_data& get_data<return_data>(data_index i) { return return_data_store.get_at(i); }

	private:
		std::optional<data_index> create_node_data(node_type t)
		{
			switch (t)
			{
			case node_type::NUMBER: return constants.create<number>();
			case node_type::STRING: return constants.create<string>();
			case node_type::BOOLEAN: return constants.create<boolean>();
			case node_type::MOVE: return move_data_store.create();
			case node_type::FUNCTION: return function_data_store.create();
			case node_type::FUNCTION_CALL: return function_call_data_store.create();
			case node_type::JMP:
			case node_type::JNZ:
				return label_store.create();
			case node_type::SDEALLOC: 
			case node_type::SALLOC: 
				return size_store.create();
			case node_type::RET:
				return return_data_store.create();
			default: return std::nullopt;
			}
		}
	};

}

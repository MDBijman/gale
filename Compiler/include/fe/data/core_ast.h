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
		BRANCH,
		WHILE_LOOP,

		BIN_OP,
		UN_OP
	};

	using data_index = size_t;
	using scope_index = size_t;
	using node_id = size_t;

	struct node
	{
		node() : kind(node_type::NOP), id(0) {}
		node(node_type t) : kind(t) {}

		node_type kind;
		node_id id;
		std::vector<node_id> children;
		std::optional<node_id> parent_id;

		std::optional<data_index> data_index;
		std::optional<scope_index> value_scope_id;

		operator std::string() const
		{
			std::string s;
			switch (kind)
			{
			case node_type::NOP:s += "NOP"; break;
			case node_type::NUMBER:s += "NUMBER"; break;
			case node_type::STRING:s += "STRING"; break;
			case node_type::BOOLEAN:s += "BOOLEAN"; break;
			case node_type::TUPLE:s += "TUPLE"; break;
			case node_type::ARRAY:s += "ARRAY"; break;
			case node_type::SALLOC:s += "SALLOC"; break;
			case node_type::SDEALLOC:s += "SDEALLOC"; break;
			case node_type::MOVE:s += "MOVE"; break;
			case node_type::FUNCTION:s += "FUNCTION"; break;
			case node_type::FUNCTION_CALL:s += "FUNCTION_CALL"; break;
			case node_type::RET:s += "RET"; break;
			case node_type::BLOCK:s += "BLOCK"; break;
			case node_type::BRANCH:s += "BRANCH"; break;
			case node_type::WHILE_LOOP:s += "WHILE_LOOP"; break;
			case node_type::BIN_OP:s += "BIN_OP"; break;
			case node_type::UN_OP:s += "UN_OP"; break;
			}
			return s;
		}
	};

	class ast
	{
		memory::dynamic_store<node> nodes;
		memory::dynamic_store<move_data> move_data_store;
		memory::dynamic_store<function_data> function_data_store;
		constants_store constants;

		node_id root;

	public:
		ast(node_type t)
		{
			root = nodes.create();
			nodes.get_at(root) = node(t);
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
			return new_node;
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
			default: return std::nullopt;
			}
		}
	};

}

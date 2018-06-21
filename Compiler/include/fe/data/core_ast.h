#pragma once
#include <vector>
#include <optional>
#include <variant>

#include "fe/data/types.h"
#include "fe/data/values.h"
#include "fe/data/value_scope.h"
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
		IDENTIFIER,
		IDENTIFIER_TUPLE,
		SET,
		FUNCTION,
		TUPLE,
		BLOCK,
		FUNCTION_CALL,
		BRANCH,
		REFERENCE,
		WHILE_LOOP
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
	};

	class ast
	{
		memory::dynamic_store<node> nodes;
		memory::dynamic_store<core_ast::identifier> identifiers;

		constants_store constants;
		stack value_scopes;

		node_id root;

	public:
		ast(node_type t)
		{
			root = nodes.create();
			nodes.get_at(root) = node(t);
			nodes.get_at(root).data_index = create_node_data(t);
			nodes.get_at(root).value_scope_id = create_value_scope();
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

		scope_index create_value_scope()
		{
			return value_scopes.create();
		}

		scope_index create_value_scope(scope_index parent)
		{
			auto new_scope = value_scopes.create();
			value_scopes.get_at(new_scope).parent = parent;
			return new_scope;
		}

		stack& get_runtime_context()
		{
			return value_scopes;
		}

		// Node data 
		template<class DataType>
		DataType& get_data(data_index i);
		template<> identifier& get_data<identifier>(data_index i) { return identifiers.get_at(i); }
		template<> boolean& get_data<boolean>(data_index i) { return constants.get<boolean>(i); }
		template<> string& get_data<string>(data_index i) { return constants.get<string>(i); }
		template<> number& get_data<number>(data_index i) { return constants.get<number>(i); }

	private:
		std::optional<data_index> create_node_data(node_type t)
		{
			switch (t)
			{
			case node_type::IDENTIFIER: return identifiers.create();
			case node_type::NUMBER: return constants.create<number>();
			case node_type::STRING: return constants.create<string>();
			case node_type::BOOLEAN: return constants.create<boolean>();
			default: return std::nullopt;
			}
		}
	};
}

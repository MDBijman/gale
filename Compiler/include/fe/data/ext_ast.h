#pragma once
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>
#include <assert.h>
#include <array>

#include "fe/data/name_scope.h"
#include "fe/data/type_scope.h"
#include "fe/data/ast_data.h"
#include "utils/memory/data_store.h"
#include "utils/memory/small_vector.h"

namespace fe::ext_ast
{
	enum class node_type : unsigned char
	{
		ASSIGNMENT,
		TUPLE,
		BLOCK,
		BLOCK_RESULT,
		FUNCTION,
		WHILE_LOOP,
		IF_STATEMENT,
		ELSEIF_STATEMENT,
		ELSE_STATEMENT,
		MATCH_BRANCH,
		MATCH,
		IDENTIFIER,
		FUNCTION_CALL,
		MODULE_DECLARATION,
		EXPORT_STMT,
		IMPORT_DECLARATION,
		DECLARATION,
		REFERENCE,
		ARRAY_VALUE,

		// Literals
		STRING,
		BOOLEAN,
		NUMBER,

		// Type declarations
		TYPE_DEFINITION,
		RECORD,
		RECORD_ELEMENT,
		IDENTIFIER_TUPLE,

		// Type expressions
		TYPE_TUPLE,
		TYPE_ATOM,
		FUNCTION_TYPE,
		REFERENCE_TYPE,
		ARRAY_TYPE,

		//// Operators
		// Logical
		AND,
		OR,
		// Math
		ADDITION,
		SUBTRACTION,
		MULTIPLICATION,
		DIVISION,
		MODULO,
		// Comparisons
		EQUALITY,
		GREATER_THAN,
		GREATER_OR_EQ,
		LESS_THAN,
		LESS_OR_EQ,
	};

	constexpr bool is_binary_op(node_type kind)
	{
		return (kind == node_type::ADDITION
			|| kind == node_type::SUBTRACTION
			|| kind == node_type::MULTIPLICATION
			|| kind == node_type::DIVISION
			|| kind == node_type::MODULO
			|| kind == node_type::EQUALITY
			|| kind == node_type::GREATER_OR_EQ
			|| kind == node_type::GREATER_THAN
			|| kind == node_type::LESS_OR_EQ
			|| kind == node_type::LESS_THAN
			|| kind == node_type::AND
			|| kind == node_type::OR);
	}

	constexpr bool is_terminal_node(node_type kind)
	{
		return (kind == node_type::IDENTIFIER
			|| kind == node_type::STRING
			|| kind == node_type::BOOLEAN
			|| kind == node_type::NUMBER);
	}

#pragma pack(push, 1)
	struct node 
	{
		node_type kind;
		node_id id;
		node_id children_id;
		node_id parent_id;

		data_index data_index;
		scope_index name_scope_id;
		// #performance we can use a single scope id instead of 2 saving 4 bytes per node
		scope_index type_scope_id;
	};
#pragma pack(pop)

	using node_children = memory::small_vector<node_id, 3>;

	struct ast_allocation_hints
	{
		size_t nodes;
		size_t children;
		size_t name_scopes;
		size_t type_scopes;
		size_t identifiers;
		size_t booleans;
		size_t strings;
		size_t numbers;
	};

	class ast
	{
		memory::dynamic_store<node> nodes;
		memory::dynamic_store<node_children> children;
		memory::dynamic_store<name_scope> name_scopes;
		memory::dynamic_store<type_scope> type_scopes;

		// Storage of node data
		memory::dynamic_store<identifier> identifiers;
		memory::dynamic_store<boolean> booleans;
		memory::dynamic_store<string> strings;
		memory::dynamic_store<number> numbers;

		node_id root;

	public:
		ast() {}
		ast(ast_allocation_hints h)
		{
			nodes.reserve(h.nodes);
			name_scopes.reserve(h.name_scopes);
			type_scopes.reserve(h.type_scopes);
			identifiers.reserve(h.identifiers);
			booleans.reserve(h.booleans);
			strings.reserve(h.strings);
			numbers.reserve(h.numbers);
		}

		void set_root_id(node_id id)
		{
			root = id;
		}

		// Root node
		node_id root_id()
		{
			return root;
		}

		// Nodes
		node_children& get_children(children_id id)
		{
			assert(id != no_children);
			return children.get_at(id);
		}

		node_children& children_of(node_id id)
		{
			assert(id != no_node);
			return children_of(get_node(id));
		}

		node_children& children_of(node& n)
		{
			return get_children(n.children_id);
		}

		node_id create_node(node_type t)
		{
			auto new_node = nodes.create();
			auto& node = get_node(new_node);
			node.id = new_node;
			node.kind = t;
			node.data_index = create_node_data(t);
			node.children_id = is_terminal_node(t) ? no_children : children.create();
			return new_node;
		}

		node& get_node(node_id id)
		{
			assert(id != no_node);
			return nodes.get_at(id);
		}

		std::optional<identifier> get_module_name()
		{
			auto module_dec_id = find_node(node_type::MODULE_DECLARATION);
			if (!module_dec_id.has_value()) return std::nullopt;
			auto& id_node = get_node(children_of(module_dec_id.value())[0]);
			return get_data<identifier>(id_node.data_index);
		}

		std::optional<std::vector<identifier>> get_imports()
		{
			auto import_dec_id = find_node(node_type::IMPORT_DECLARATION);
			if (!import_dec_id.has_value()) return std::nullopt;

			std::vector<identifier> imports;
			auto& children = children_of(import_dec_id.value());
			for (auto child : children)
			{
				auto& id_node = get_node(child);
				imports.push_back(get_data<identifier>(id_node.data_index));
			}
			return imports;
		}

		// Scopes
		scope_index create_name_scope()
		{
			return name_scopes.create();
		}

		scope_index create_name_scope(scope_index parent)
		{
			assert(parent != no_scope);
			auto new_scope = name_scopes.create();
			name_scopes.get_at(new_scope).set_parent(parent);
			return new_scope;
		}

		name_scope& get_name_scope(scope_index id)
		{
			assert(id != no_scope);
			return name_scopes.get_at(id);
		}

		name_scope::get_scope_cb name_scope_cb()
		{
			return [&](scope_index i) { return &name_scopes.get_at(i); };
		}

		scope_index create_type_scope()
		{
			return type_scopes.create();
		}

		scope_index create_type_scope(scope_index parent)
		{
			assert(parent != no_scope);
			auto new_scope = type_scopes.create();
			type_scopes.get_at(new_scope).set_parent(parent);
			return new_scope;
		}

		type_scope& get_type_scope(scope_index id)
		{
			assert(id != no_scope);
			return type_scopes.get_at(id);
		}

		type_scope::get_scope_cb type_scope_cb()
		{
			return [&](scope_index i) { return &type_scopes.get_at(i); };
		}

		// Node data 
		template<class DataType>
		DataType& get_data(data_index i);
		template<> identifier& get_data<identifier>(data_index i) { assert(i != no_data); return identifiers.get_at(i); }
		template<> boolean&    get_data<boolean>(data_index i) { assert(i != no_data); return booleans.get_at(i); }
		template<> string&     get_data<string>(data_index i) { assert(i != no_data); return strings.get_at(i); }
		template<> number&     get_data<number>(data_index i) { assert(i != no_data); return numbers.get_at(i); }

	private:
		data_index create_node_data(node_type t)
		{
			switch (t)
			{
			case node_type::IDENTIFIER: return identifiers.create();
			case node_type::NUMBER:     return numbers.create();
			case node_type::STRING:     return strings.create();
			case node_type::BOOLEAN:    return booleans.create();
			default:
				if (is_binary_op(t)) return strings.create();
				return no_data;
			}
		}

		std::optional<node_id> find_node(node_type t)
		{
			for (node_id i = 0; i < nodes.get_data().size(); i++)
			{
				if (!nodes.is_occupied(i))
					continue;
				if (nodes.get_at(i).kind == t)
					return i;
			}
			return std::nullopt;
		}
	};
}

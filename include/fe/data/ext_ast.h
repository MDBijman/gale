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
#include "fe/data/ext_ast_data.h"

namespace fe::ext_ast
{
	enum class node_type
	{
		ASSIGNMENT,
		TUPLE,
		BLOCK,
		BLOCK_RESULT,
		FUNCTION,
		WHILE_LOOP,
		IF_STATEMENT,
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
		ATOM_DECLARATION,
		TUPLE_DECLARATION,
		IDENTIFIER_TUPLE,

		// Type expressions
		TYPE_TUPLE,
		TYPE_ATOM,
		FUNCTION_TYPE,
		REFERENCE_TYPE,
		ARRAY_TYPE,

		// (Math) operators
		ADDITION,
		SUBTRACTION,
		MULTIPLICATION,
		DIVISION,
		MODULO,
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
			|| kind == node_type::LESS_THAN);
	}

	/*
	* The index of a piece of data in a data store. Each node type that contains data has its own data type
	* and corresponding data store. If a node type has no data, then the index will be the maximum value,
	* i.e. std::numeric_limits<data_index>::max().
	*/
	using data_index = size_t;
	using type_index = size_t;
	using scope_index = size_t;
	using node_id = size_t;

	struct node
	{
		node() {}
		node(node_type t) : kind(t) {}
		node(node_type t, data_index i) : kind(t), data_index(i) {}
		node(node_type t, data_index i, std::vector<node_id> children) : kind(t), data_index(i), children(children) {}
		node(node_type t, std::vector<node_id> children) : kind(t), children(children) {}

		node_type kind;
		std::vector<node_id> children;
		std::optional<node_id> parent_id;

		std::optional<types::unique_type> type;
		std::optional<data_index> data_index;
		std::optional<scope_index> name_scope_id;
		std::optional<scope_index> type_scope_id;
	};

	namespace detail
	{
		template<class T, size_t SIZE>
		class data_store
		{
			std::array<T, SIZE> data;
			std::array<bool, SIZE> occupieds;
		public:
			data_store() : occupieds({ false }) {}

			data_index create()
			{
				auto free_pos = std::find(occupieds.begin(), occupieds.end(), false);
				*free_pos = true;
				return std::distance(occupieds.begin(), free_pos);
			}

			T& get_at(data_index i)
			{
				assert(occupieds[i]);
				return data.at(i);
			}

			void free_at(data_index i)
			{
				occupieds[i] = false;
			}

			const std::array<T, SIZE>& get_data()
			{
				return data;
			}
		};
	}

	class ast
	{
		detail::data_store<node, 256> nodes;
		detail::data_store<name_scope, 64> name_scopes;
		detail::data_store<type_scope, 64> type_scopes;

		// Storage of node data
		detail::data_store<identifier, 64> identifiers;
		detail::data_store<boolean, 64> booleans;
		detail::data_store<string, 64> strings;
		detail::data_store<number, 64> numbers;

		node_id root;

	public:
		ast(node_type t)
		{
			root = nodes.create();
			nodes.get_at(root) = node(t);
			nodes.get_at(root).data_index = create_node_data(t);
			nodes.get_at(root).name_scope_id = create_name_scope();
			nodes.get_at(root).type_scope_id = create_type_scope();
		}

		// Root node
		node_id root_id()
		{
			return root;
		}

		// Nodes
		node_id create_node(node_type t)
		{
			auto new_node = nodes.create();
			get_node(new_node).kind = t;
			get_node(new_node).data_index = create_node_data(t);
			return new_node;
		}

		node& get_node(node_id id)
		{
			return nodes.get_at(id);
		}

		std::optional<identifier> get_module_name()
		{
			auto module_dec_id = find_node(node_type::MODULE_DECLARATION);
			if (!module_dec_id.has_value()) return std::nullopt;
			auto& module_dec_node = get_node(module_dec_id.value());
			auto& id_node = get_node(module_dec_node.children[0]);
			return get_data<identifier>(id_node.data_index.value());
		}

		std::optional<std::vector<identifier>> get_imports()
		{
			auto import_dec_id = find_node(node_type::IMPORT_DECLARATION);
			if (!import_dec_id.has_value()) return std::nullopt;
			auto& module_dec_node = get_node(import_dec_id.value());

			std::vector<identifier> imports;
			for (auto child : module_dec_node.children)
			{
				auto& id_node = get_node(child);
				imports.push_back(get_data<identifier>(id_node.data_index.value()));
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
			auto new_scope = name_scopes.create();
			name_scopes.get_at(new_scope).set_parent(&name_scopes.get_at(parent));
			return new_scope;
		}

		name_scope& get_name_scope(scope_index id)
		{
			return name_scopes.get_at(id);
		}

		scope_index create_type_scope()
		{
			return type_scopes.create();
		}

		scope_index create_type_scope(scope_index parent)
		{
			auto new_scope = type_scopes.create();
			type_scopes.get_at(new_scope).set_parent(&type_scopes.get_at(parent));
			return new_scope;
		}

		type_scope& get_type_scope(scope_index id)
		{
			return type_scopes.get_at(id);
		}

		// Node data 
		template<class DataType>
		DataType& get_data(data_index i);
		template<> identifier& get_data<identifier>(data_index i) { return identifiers.get_at(i); }
		template<> boolean&    get_data<boolean>(data_index i) { return booleans.get_at(i); }
		template<> string&     get_data<string>(data_index i) { return strings.get_at(i); }
		template<> number&     get_data<number>(data_index i) { return numbers.get_at(i); }

	private:
		std::optional<data_index> create_node_data(node_type t)
		{
			switch (t)
			{
			case node_type::IDENTIFIER: return identifiers.create();
			case node_type::NUMBER:     return numbers.create();
			case node_type::STRING:     return strings.create();
			case node_type::BOOLEAN:    return booleans.create();
			default:                    return std::nullopt;
			}
		}

		std::optional<node_id> find_node(node_type t)
		{
			for (node_id i = 0; i < nodes.get_data().size(); i++)
			{
				if (nodes.get_at(i).kind == t)
					return i;
			}
			return std::nullopt;
		}
	};
}

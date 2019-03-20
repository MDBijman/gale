#pragma once
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>
#include <assert.h>
#include <array>

#include "fe/data/constants_store.h"
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

		// Declarations
		TYPE_DEFINITION,
		IDENTIFIER_TUPLE,

		// Type expressions
		TUPLE_TYPE,
		ATOM_TYPE,
		FUNCTION_TYPE,
		REFERENCE_TYPE,
		ARRAY_TYPE,
		SUM_TYPE,

		//// Operators
		// Logical
		AND,
		OR,
		NOT,
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
		// Array
		ARRAY_ACCESS
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

	constexpr bool is_unary_op(node_type kind)
	{
		return (kind == node_type::NOT);
	}

	constexpr bool is_type_node(node_type kind)
	{
		return (kind == node_type::ATOM_TYPE
			|| kind == node_type::FUNCTION_TYPE
			|| kind == node_type::TUPLE_TYPE
			|| kind == node_type::REFERENCE_TYPE
			|| kind == node_type::ARRAY_TYPE
			|| kind == node_type::SUM_TYPE);
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

		data_index_t data_index;
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

	class ast;

	namespace detail
	{
		template<class DataType>
		DataType& get_data(ast& ast, data_index_t i);
	}

	class ast
	{
		memory::dynamic_store<node> nodes;
		memory::dynamic_store<node_children> children;
		memory::dynamic_store<name_scope> name_scopes;
		memory::dynamic_store<type_scope> type_scopes;
		memory::dynamic_store<identifier> identifiers;

		constants_store constants;

		node_id root;

	public:
		ast();
		ast(ast_allocation_hints h);

		void set_root_id(node_id id);

		// Root node
		node_id root_id();

		// Nodes
		node_children& get_children(children_id id);
		node_children& children_of(node_id id);
		node_children& children_of(node& n);
		node_id create_node(node_type t);
		node& get_node(node_id id);

		std::optional<identifier> get_module_name();
		std::optional<std::vector<identifier>> get_imports();

		// Scopes
		scope_index create_name_scope();
		scope_index create_name_scope(scope_index parent);
		name_scope& get_name_scope(scope_index id);
		name_scope::get_scope_cb name_scope_cb();

		scope_index create_type_scope();
		scope_index create_type_scope(scope_index parent);
		type_scope& get_type_scope(scope_index id);
		type_scope::get_scope_cb type_scope_cb();

		// Node data 
		constants_store& get_constants();

		template<class DataType>
		DataType& get_data(data_index_t i)
		{
			return detail::get_data<DataType>(*this, i);
		}
		template<class DataType>
		friend DataType& detail::get_data(ast& a, data_index_t t);

	private:
		data_index_t create_node_data(node_type t);
		std::optional<node_id> find_node(node_type t);
	};

	namespace detail
	{
		template<> identifier& get_data<identifier>(ast& a, data_index_t i);
		template<> boolean&    get_data<boolean>(ast& a, data_index_t i);
		template<> string&     get_data<string>(ast& a, data_index_t i);
		template<> number&     get_data<number>(ast& a, data_index_t i);
	}
}

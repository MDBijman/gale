#pragma once
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <unordered_map>
#include <assert.h>

namespace fe::ext_ast
{
	enum class node_type
	{
		ASSIGNMENT,
		TUPLE,
		BLOCK,
		FUNCTION,
		WHILE_LOOP,
		IF_STATEMENT,
		MATCH_BRANCH,
		MATCH,
		IDENTIFIER,
		LITERAL,
		FUNCTION_CALL,
		MODULE_DECLARATION,
		EXPORT_STMT,
		DECLARATION,
		ATOM_DECLARATION,
		TUPLE_DECLARATION,
		TYPE_DEFINITION,
		IDENTIFIER_TUPLE,
		TYPE_TUPLE,
		TYPE_ATOM,
		FUNCTION_TYPE,
		REFERENCE_TYPE,
		ARRAY_TYPE,
		REFERENCE,
		ARRAY_VALUE,
		IMPORT_DECLARATION
	};

	/*
	* The index of a piece of data in a data store. Each node type that contains data has its own data type
	* and corresponding data store. If a node type has no data, then the index will be the maximum value,
	* i.e. std::numeric_limits<data_index>::max().
	*/
	using data_index = size_t;
	using scope_index = size_t;
	using node_id = size_t;

	struct node
	{
		node(node_type t) : type(t) {}
		node(node_type t, data_index i) : type(t), data_index(i) {}
		node(node_type t, data_index i, std::vector<node_id> children) : type(t), data_index(i), children(children) {}
		node(node_type t, std::vector<node_id> children) : type(t), children(children) {}

		node_type type;
		std::vector<node_id> children;
		std::optional<node_id> parent_id;
		std::optional<data_index> data_index;
		std::optional<scope_index> scope_index;
	};

	// Node datatypes
	struct identifier
	{
		std::vector<std::string> segments;
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
		};

		struct nested_type
		{
			nested_type();

			void insert(std::string field, nested_type t);

			void insert(std::string field);

			std::optional<std::vector<int>> resolve(const identifier& name) const;

			std::vector<std::variant<std::string, std::pair<std::string, nested_type>>> names;
		};

		struct type_lookup_res
		{
			std::size_t scope_distance;
			nested_type type_structure;
		};

		struct var_lookup_res
		{
			std::size_t scope_distance;
			const identifier& type_name;
		};

		struct scope
		{
			/*
			* The identifiers in a scope are all named variables that can be referenced from within that scope.
			* The name of the type is also stored, for resolving nested field references later.
			*/
			std::unordered_map<std::string, std::pair<identifier, bool>> identifiers;

			/*
			* The nested types in a scope include all type declarations that contain a named variable within it
			* that can be referenced. When a new variable is declared of a type that is nested, all the inner types must
			* be resolvable within the same scope.
			*
			* Example:
			*	# Nested type declaration
			*	type Pair = (std.i32 a, std.i32 b)
			*	# New nested variable declaration
			*	var x = Pair (1, 2);
			*
			* In the example above, the names x.a and x.b must be resolvable. To enable this, when the name resolver
			* encounters the Pair type definition, it adds the nested names a and b to this map. When the variable x is
			* defined, 'Pair' is found in this map, causing x.a and x.b to be added to the scope.
			*/
			std::unordered_map<std::string, nested_type> nested_types;

			/*
			* Declares the variable with the given name within this scope. The variable will not yet be resolvable.
			*/
			void declare_variable(std::string id, const identifier& type_name);

			/*
			* Defines the given name within this scope. After this, the variable will be resolvable.
			*/
			void define_variable(const std::string& id);

			/*
			* Returns the type name of the given reference.
			*/
			std::optional<var_lookup_res> resolve_variable(const identifier& id) const;

			/*
			* Defines the given name within this scope as the type defined in the node,
			* type references with the name will be resolvable.
			*/
			void define_type(std::string id, const node& t);

			/*
			* Defines the given name within this scope as the type given.
			* After this, type references with the name will be resolvable.
			*/
			void define_type(std::string id, const nested_type& t);

			/*
			* Returns the type data of the type with the given name if it exists.
			*/
			std::optional<type_lookup_res> resolve_type(const identifier& id) const;
		};
	}

	class ast
	{
		detail::data_store<node, 256> nodes;
		detail::data_store<detail::scope, 64> scopes;

		// Storage of node data
		detail::data_store<identifier, 64> identifiers;

		node_id root;

	public:
		ast(node_type t)
		{
			root = nodes.create();
			nodes.get_at(root) = node(t, create_node_data(t).value());
			nodes.get_at(root).scope_index = create_scope();
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
			get_node(new_node).type = t;
			get_node(new_node).data_index = create_node_data(t);
			return new_node;
		}

		node& get_node(node_id id)
		{
			return nodes.get_at(id);
		}

		// Scopes
		scope_index create_scope()
		{
			return scopes.create();
		}

		detail::scope& get_scope(scope_index id)
		{
			return scopes.get_at(id);
		}

		// Node data getting and creating

		template<class DataType>
		DataType& get_data(data_index i);

		template<> identifier& get_data<identifier>(data_index i) { return identifiers.get_at(i); }

	private:
		std::optional<data_index> create_node_data(node_type t)
		{
			switch (t)
			{
			case node_type::IDENTIFIER:
				return identifiers.create();
			default:
				return std::nullopt;
			}
		}
	};
}

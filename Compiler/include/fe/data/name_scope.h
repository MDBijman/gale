#pragma once
#include <unordered_map>
#include <optional>
#include <vector>
#include <variant>
#include <functional>
#include "fe/data/ast_data.h"

namespace fe::ext_ast
{
	class name_scope
	{
	public:
		struct type_lookup
		{
			std::size_t scope_distance;
			node_id type_node;
		};

		struct var_lookup
		{
			std::size_t scope_distance;
			std::optional<node_id> type_node;
			uint32_t unique_id;
		};

		using get_scope_cb = std::function<name_scope*(scope_index)>;

	private:
		/*
		* The identifiers in a scope are all named variables that can be referenced from within that scope.
		* The name of the type is also stored, for resolving nested field references later.
		*/
		std::unordered_map<name, std::pair<node_id, bool>> variables;

		std::unordered_map<name, bool> opaque_variables;

		// Unique ids of variable names
		std::unordered_map<name, uint32_t> variable_ids;

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
		std::unordered_map<name, node_id> types;

		std::unordered_map<module_name, scope_index> modules;

		// Parent scope
		std::optional<scope_index> parent;

		// The id counter is used to generate unique ids for identifiers
		// Only the root name scope should generate these ids to avoid collisions
		uint32_t id_counter = 0;

	public:

		/*
		* Adds all variables, types, and modules to this scope.
		*/
		void merge(name_scope other);

		void set_parent(scope_index other);

		size_t depth(get_scope_cb);

		uint32_t generate_unique_id(get_scope_cb);

		/*
		* Adds the scope to this module accessible through the module_name.
		*/
		void add_module(module_name, scope_index scope);

		// Variable names

		/*
		* Declares the variable within this scope, with the node begin the type node of the variable.
		* The variable will not yet be resolvable.
		*/
		void declare_variable(name, node_id node);

		/*
		* Declares a variable with no accessible fields.
		* The variable will not yet be resolvable.
		*/
		void declare_variable(name);

		/*
		* Defines the given name within this scope. After this, the variable will be resolvable.
		*/
		void define_variable(name);

		/*
		* Returns the type name of the given reference.
		*/
		std::optional<var_lookup> resolve_variable(module_name, name var, get_scope_cb) const;
		std::optional<var_lookup> resolve_variable(name, get_scope_cb) const;

		// Type names

		/*
		* Defines the given name within this scope as the type given, with the node being the type expression.
		* After this, type references with the name will be resolvable.
		*/
		void define_type(name n, node_id t);

		/*
		* Returns the type data of the type with the given name if it exists.
		*/
		std::optional<type_lookup> resolve_type(module_name, name, get_scope_cb) const;
		std::optional<type_lookup> resolve_type(name, get_scope_cb) const;
	};
}

#pragma once
#include <unordered_map>
#include <optional>
#include <vector>
#include <variant>
#include "fe/data/ast_data.h"

namespace fe::ext_ast
{
	using name = std::string;

	class name_scope
	{
		struct type_lookup
		{
			std::size_t scope_distance;
			node_id type_node;
		};

		struct var_lookup
		{
			std::size_t scope_distance;
			std::optional<node_id> type_node;
		};

		/*
		* The identifiers in a scope are all named variables that can be referenced from within that scope.
		* The name of the type is also stored, for resolving nested field references later.
		*/
		std::unordered_map<name, std::pair<node_id, bool>> variables;

		std::unordered_map<name, bool> opaque_variables;

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

		std::unordered_map<identifier, name_scope*> modules;

		// Parent scope
		std::optional<name_scope*> parent;

		
	public:
		/*
		* Adds all variables, types, and modules to this scope.
		*/
		void merge(name_scope other);

		void set_parent(name_scope* other);

		/*
		* Adds the scope to this module accessible through the module_name.
		*/
		void add_module(const identifier& module_name, name_scope* scope);

		// Variable names

		/*
		* Declares the variable within this scope, with the node begin the type node of the variable. 
		* The variable will not yet be resolvable.
		*/
		void declare_variable(const name& id, node_id node);

		/*
		* Declares a variable with no accessible fields.
		* The variable will not yet be resolvable.
		*/
		void declare_variable(const name& id);

		/*
		* Defines the given name within this scope. After this, the variable will be resolvable.
		*/
		void define_variable(const name& id);

		/*
		* Returns the type name of the given reference.
		*/
		std::optional<var_lookup> resolve_variable(const identifier& module, const name& var) const;
		std::optional<var_lookup> resolve_variable(const name& var) const;

		// Type names

		/*
		* Defines the given name within this scope as the type given, with the node being the type expression.
		* After this, type references with the name will be resolvable.
		*/
		void define_type(const name& n, node_id t);

		/*
		* Returns the type data of the type with the given name if it exists.
		*/
		std::optional<type_lookup> resolve_type(const identifier& module, const name& var) const;
		std::optional<type_lookup> resolve_type(const name& var) const;
	};
}

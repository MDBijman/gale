#pragma once
#include "fe/pipeline/error.h"

#include <vector>
#include <unordered_map>
#include <variant>
#include <string>
#include <optional>
#include <tuple>
#include <stack>
#include <memory>

namespace fe::extended_ast
{
	struct node;
	struct identifier;

	using unique_node = std::unique_ptr<node>;
}

namespace fe::resolution
{
	struct nested_type
	{
		nested_type(); 

		void insert(std::string field, nested_type t);

		void insert(std::string field);

		std::optional<std::vector<int>> resolve(const extended_ast::identifier& name) const;

		std::vector<std::variant<std::string, std::pair<std::string, nested_type>>> names;
	};
}

namespace fe::resolution::detail
{
	struct type_lookup_res
	{
		std::size_t scope_distance;
		nested_type type_structure;
	};

	struct var_lookup_res
	{
		std::size_t scope_distance;
		const extended_ast::identifier& type_name;
	};

	/*
	* A scope contains all variables that have been declared within it, and if a variable has been defined.
	* Declaration happens when a name appears on the lhs of an assignment, or within a parameter list.
	* A variable is not defined in the rhs of its assignment when it has not been defined earlier.
	* The exception to this rule is functions, to allow recursion.
	*
	* Example:
	*	# Legal, y is declared and defined when it is referenced on the rhs of the assignment to z.
	*	var y = 1;
	*	var z = y;
	*	# Illegal, x is declared but not defined on the right hand side
	*	var x = x;
	*	# Legal, m is defined already when it is referenced on the rhs
	*	var m = 1;
	*	var m = m + 1;
	*	# Legal, functions are exceptions to the rule
	*	fn fact std.i32 a -> std.i32 = a match {
	*		| a == 1 -> a
	*		| 1 == 1 -> a * fact (a - 1)
	*	};
	*/
	class scoped_node 
	{
	public:
		scoped_node() = default;
		scoped_node(const scoped_node& other);
		scoped_node& operator=(const scoped_node&);

		/*
		* Merges this scope with the given scope, without changing the identifiers in the other scope.
		*/
		void merge(scoped_node& other);

		/*
		* Merges this scope with the given scope, prefixing the given segments to all identifiers in the other scope.
		*/
		void merge(std::vector<std::string> name, scoped_node& other);

		/*
		* Merges this scope with the given scope, prefixing the given segment to all identifiers in the other scope.
		*/
		void merge(std::string name, scoped_node& other);

		/*
		* Declares the variable with the given name within this scope. The variable will not yet be resolvable.
		*/
		void declare_var_id(std::string id, const extended_ast::identifier& type_name);

		/*
		* Defines the given name within this scope. After this, the variable will be resolvable.
		*/
		void define_var_id(const std::string& id);

		/*
		* Returns the type name of the given reference.
		*/
		std::optional<var_lookup_res> resolve_var_id(const extended_ast::identifier& id) const;

		/*
		* Defines the given name within this scope as the type defined in the node,
		* type references with the name will be resolvable.
		*/
		void define_type(std::string id, const extended_ast::unique_node& t);

		/*
		* Defines the given name within this scope as the type given.
		* After this, type references with the name will be resolvable.
		*/
		void define_type(std::string id, const nested_type& t);

		/*
		* Returns the type data of the type with the given name if it exists.
		*/
		std::optional<type_lookup_res> resolve_type(const extended_ast::identifier& id) const;

	private:
		/*
		* The identifiers in a scope are all named variables that can be referenced from within that scope.
		* The name of the type is also stored, for resolving nested field references later.
		*/
		std::unordered_map<
			std::string,
			std::pair<extended_ast::identifier, bool>
		> identifiers;

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
	};

	struct var_resolve_res
	{
		std::size_t scope_distance;
		std::vector<int> offsets;
	};

	using type_resolve_res = detail::type_lookup_res;

}

namespace fe::resolution
{
	using scope_tree_node = detail::node;
	using unscoped_node = detail::unscoped_node;
	using scoped_node = detail::scoped_node;
	//using scope_tree_iterator = detail::node_iterator;

	class scope_environment
	{
		/*public:
			using iterator = scope_tree_iterator;

			iterator begin()
			{
				return iterator(&root);
			}

			iterator end()
			{
				return iterator(nullptr);
			}
	*/
	public:
		scope_environment();

		scope_environment(std::unique_ptr<scope_tree_node> root);

		scope_environment(scope_environment&&);

		scope_environment(const scope_environment&);

		scope_tree_node& get_root();

		void set_root(std::unique_ptr<scope_tree_node>);

	private:
		std::unique_ptr<scope_tree_node> root;

		std::unordered_map<std::string, scoped_node> modules;
	};
}

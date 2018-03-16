#pragma once
#include "fe/pipeline/error.h"
#include "fe/data/extended_ast.h"

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>

namespace fe
{
	struct nested_type
	{
		void insert(std::string field, nested_type t)
		{
			names.push_back(std::pair<std::string, nested_type>(field, t));
		}

		void insert(std::string field)
		{
			names.push_back(field);
		}

		std::optional<std::vector<int>> resolve(const extended_ast::identifier& name) const
		{
			if (name.segments.size() == 0) return std::nullopt;

			for (auto it = names.begin(); it != names.end(); it++)
			{
				auto& elem = *it;

				if (std::holds_alternative<std::string>(elem))
				{
					if (name.segments.size() > 1) continue;

					auto field = std::get<std::string>(elem);
					if (field == name.segments.at(0))
						return std::vector<int>({ static_cast<int>(std::distance(names.begin(), it)) });
				}
				else
				{
					auto nested = std::get<std::pair<std::string, nested_type>>(elem);
					if (nested.first != name.segments.at(0))
						continue;

					if (name.segments.size() != 1)
					{
						auto nested_resolved = nested.second.resolve(name.without_first_segment());
						if (!nested_resolved.has_value()) return std::nullopt;
						nested_resolved.value().insert(nested_resolved.value().begin(), 
							static_cast<int>(std::distance(names.begin(), it)));
						return nested_resolved.value();
					}
					else
					{
						return std::vector<int>({ static_cast<int>(std::distance(names.begin(), it)) });
					}
				}
			}

			return std::nullopt;
		}

		std::vector<std::variant<std::string, std::pair<std::string, nested_type>>> names;
	};
}

namespace fe::detail
{
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
	*	var fact = fn std.i32 a -> std.i32 = a match {
	*		| a == 1 -> a
	*		| 1 == 1 -> a * fact (a - 1)
	*	};
	*/
	class scope
	{
	public:
		scope() {}
		scope(const scope& other) : identifiers(other.identifiers), parent(other.parent)
		{
			for (decltype(auto) pair : other.nested_types)
				this->nested_types.insert(pair);
		}

		void merge(const scope& other)
		{
			for (decltype(auto) id : other.identifiers)
				this->identifiers.insert(id);
			for (decltype(auto) pair : other.nested_types)
				this->nested_types.insert(pair);
		}

		void merge(const std::string& name, const scope& other)
		{
			for (decltype(auto) id : other.identifiers)
			{
				auto new_id = id.first;
				new_id.segments.insert(new_id.segments.begin(), name);
				this->identifiers.insert({ new_id, id.second });
			}
			for (decltype(auto) pair : other.nested_types)
			{
				auto new_id = pair.first;
				new_id.segments.insert(new_id.segments.begin(), name);
				this->nested_types.insert({ new_id, pair.second });
			}
		}

		void set_parent(scope& other)
		{
			this->parent = std::ref(other);
		}

		/*
		* TODO
		*/
		std::optional<std::vector<int>> resolve_reference(const extended_ast::identifier& id) const
		{
			if (id.segments.size() == 0)
				return std::nullopt;

			const auto loc = identifiers.find(extended_ast::identifier({ id.segments.at(0) }));
			if (loc == identifiers.end()) return std::nullopt;

			// The variable has been declared but not defined 
			if (loc->second.second == false)
				throw resolution_error{ "Variable referenced in its own definition" };

			// If nested reference
			if (id.segments.size() > 1)
			{
				auto& type_name = loc->second.first;
				if (auto type_loc = nested_types.find(type_name); type_loc != nested_types.end())
				{
					return type_loc->second.resolve(id.without_first_segment());
				}
				else if (parent.has_value())
				{
					auto type_contents = parent.value().get().resolve_type(type_name);
					return type_contents.value().second.resolve(id.without_first_segment());
				}
				else
				{
					return std::nullopt;
				}
			}
			else
			{
				return std::vector<int>{};
			}
		}

		std::optional<std::pair<std::size_t, nested_type>> resolve_type(const extended_ast::identifier& id) const
		{
			auto type_location = nested_types.find(id);
			if (type_location == nested_types.end())
			{
				if (parent.has_value())
				{
					auto parent_resolve = parent.value().get().resolve_type(id);

					if (parent_resolve.has_value())
						return std::pair<std::size_t, nested_type>(parent_resolve.value().first + 1,
							parent_resolve.value().second);
				}

				return std::nullopt;
			}
			else
			{
				return std::pair<std::size_t, nested_type>(0, type_location->second);
			}
		}

		/*
		* Declares the variable with the given name within this scope. The variable will not yet be resolvable.
		*/
		void declare_reference(extended_ast::identifier id, extended_ast::identifier type_name)
		{
			identifiers.insert({ std::move(id), { std::move(type_name), false } });
		}

		/*
		* Defines the given name within this scope. After this, the variable will be resolvable.
		*/
		void define_reference(const extended_ast::identifier& id)
		{
			identifiers.at(id).second = true;
		}

		/*
		* TODO
		*/
		void define_type(extended_ast::identifier id, nested_type t)
		{
			nested_types.insert({ std::move(id), std::move(t) });
		}

		void define_type(const extended_ast::identifier& id, const extended_ast::unique_node& t)
		{
			// This function builds a nested_type object from an AST node
			using field_type = std::variant<nested_type, std::string, std::pair<std::string, nested_type>>;
			std::function<field_type(extended_ast::node*)> build_fields = [&](extended_ast::node* type) -> field_type
			{
				if (auto tuple_declaration = dynamic_cast<extended_ast::tuple_declaration*>(type))
				{
					nested_type t;
					for (auto i = 0; i < tuple_declaration->elements.size(); i++)
					{
						decltype(auto) tuple_declaration_element = tuple_declaration->elements.at(i);
						auto fields = build_fields(tuple_declaration_element.get());

						if (std::holds_alternative<std::string>(fields))
							t.insert(std::get<std::string>(fields));
						else if (std::holds_alternative<nested_type>(fields))
						{
							auto nested_fields = std::get<nested_type>(fields);
							for (auto& field : nested_fields.names)
							{
								if (std::holds_alternative<std::string>(field))
								{
									t.insert(std::get<std::string>(field));
								}
								else if (std::holds_alternative<std::pair<std::string, nested_type>>(field))
								{
									auto[name, content] = std::get<std::pair<std::string, nested_type>>(field);
									t.insert(name, content);
								}
							}
						}
						else if (std::holds_alternative<std::pair<std::string, nested_type>>(fields))
						{
							auto nested_fields = std::get<std::pair<std::string, nested_type>>(fields);
							t.insert(nested_fields.first, nested_fields.second);
						}
					}
					return t;
				}
				else if (auto atom_declaration = dynamic_cast<extended_ast::atom_declaration*>(type))
				{
					auto scope_depth = build_fields(atom_declaration->type_expression.get());

					return std::pair<std::string, nested_type>(atom_declaration->name.segments.at(0),
						std::get<nested_type>(scope_depth));
				}
				else if (auto atom = dynamic_cast<extended_ast::type_atom*>(type))
				{
					auto type_name = dynamic_cast<extended_ast::identifier*>(atom->type.get());
					auto scope_depth = resolve_type(*type_name);
					return scope_depth.value().second;
				}
				else if (auto ref = dynamic_cast<extended_ast::reference_type*>(type))
				{
					return build_fields(ref->child.get());
				}
				else if (auto arr = dynamic_cast<extended_ast::array_type*>(type))
				{
					return build_fields(arr->child.get());
				}
			};

			auto nt = build_fields(t.get());
			if (std::holds_alternative<std::string>(nt))
			{
				nested_type t;
				t.insert(std::get<std::string>(nt));
				nested_types.insert({ id, t });
			}
			else if (std::holds_alternative<nested_type>(nt))
			{
				nested_types.insert({ id, std::get<nested_type>(nt) });
			}
			else if (std::holds_alternative<std::pair<std::string, nested_type>>(nt))
			{
				auto res = std::get<std::pair<std::string, nested_type>>(nt);
				nested_type t;
				t.insert(res.first, res.second);
				nested_types.insert({ id, std::get<nested_type>(res) });
			}
		}

	private:
		/*
		* The parent scope of this scope can be used to lookup if a type name exists that is accessible from within
		* this scope.
		*/
		std::optional<std::reference_wrapper<scope>> parent;

		/*
		* The identifiers in a scope are all named variables that can be referenced from within that scope.
		* The name of the type is also stored, for resolving nested field references later.
		*/
		std::unordered_map<
			extended_ast::identifier,
			std::pair<extended_ast::identifier, bool>,
			node_hasher<extended_ast::identifier>
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
		std::unordered_map<extended_ast::identifier, nested_type, node_hasher<extended_ast::identifier>> nested_types;
	};
}

namespace fe
{
	class scope_environment
	{
	public:
		scope_environment() { push(); }

		void push()
		{
			scopes.push_back(detail::scope{});

			// There is a parent
			if (scopes.size() > 1) {
				auto& parent = scopes.at(scopes.size() - 2);
				scopes.back().set_parent(parent);
			}
		}

		void pop()
		{
			scopes.pop_back();
		}

		std::optional<std::pair<std::size_t, std::vector<int>>> resolve_reference(const extended_ast::identifier& id) const
		{
			for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
			{
				auto offsets = it->resolve_reference(id);
				if (offsets.has_value())
				{
					return std::pair<std::size_t, std::vector<int>>(
						std::distance(scopes.rbegin(), it), offsets.value()
					);
				}
			}

			return std::nullopt;
		}

		std::optional<std::size_t> resolve_type(const extended_ast::identifier& id) const
		{
			for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
			{
				auto offsets = it->resolve_type(id);
				if (offsets.has_value()) return offsets.value().first;
			}

			return std::nullopt;
		}

		void define_type(const extended_ast::identifier& id, const extended_ast::unique_node& content)
		{
			scopes.rbegin()->define_type(id, content);
		}

		void define_type(const extended_ast::identifier& id, const nested_type& content)
		{
			scopes.rbegin()->define_type(id, content);
		}

		void declare(extended_ast::identifier id, extended_ast::identifier type_name)
		{
			scopes.rbegin()->declare_reference(std::move(id), std::move(type_name));
		}

		void define(const extended_ast::identifier_tuple& id)
		{
			for (auto& child : id.content)
				std::visit([this](auto& c) { this->define(c); }, child);
		}

		void define(const extended_ast::identifier& id)
		{
			scopes.rbegin()->define_reference(id);
		}

		void add_global_module(const scope_environment& mod)
		{
			this->scopes.front().merge(mod.scopes.front());
		}

		void add_module(const std::string& name, const scope_environment& mod)
		{
			this->scopes.front().merge(name, mod.scopes.front());
		}

	private:
		std::vector<detail::scope> scopes;
	};
}

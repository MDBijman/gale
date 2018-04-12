#pragma once
#include <unordered_map>
#include <string>

#include "fe/data/types.h"
#include "fe/data/ext_ast_data.h"

namespace fe::ext_ast
{
	using name = std::string;

	class type_scope
	{
		struct var_lookup
		{
			long long scope_distance;
			types::type& type;
		};

		struct type_lookup
		{
			long long scope_distance;
			types::type& type;
		};

		// The types of variables
		std::unordered_map<name, types::unique_type> variables;

		// The defined types
		std::unordered_map<name, types::unique_type> types;

		std::unordered_map<identifier, type_scope*> modules;

		// Parent scope
		std::optional<type_scope*> parent;

		types::type& resolve_offsets(const std::vector<size_t>& offsets, types::type* t, size_t cur = 0);

	public:
		type_scope() {}
		type_scope(type_scope* p) : parent(p) {}
		type_scope(const type_scope& o)
		{
			for (const auto& variable : o.variables)
			{
				variables.insert({ variable.first, types::unique_type(variable.second->copy()) });
			}

			for (const auto& type : o.types)
			{
				types.insert({ type.first, types::unique_type(type.second->copy()) });
			}
		}
		type_scope(type_scope&& o) : variables(std::move(o.variables)), types(std::move(o.types)) {}

		void add_module(const identifier& module_name, type_scope* scope);

		void set_parent(type_scope* other);

		void merge(type_scope other);

		// Types of variables

		void set_type(const name& n, types::unique_type t);
		std::optional<var_lookup> resolve_variable(const identifier& n);

		// Defined types

		void define_type(const name& n, types::unique_type t);
		std::optional<type_lookup> resolve_type(const identifier& n);
	};
}
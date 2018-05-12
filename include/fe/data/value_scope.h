#pragma once
#include <optional>
#include <unordered_map>

#include "fe/data/ast_data.h"
#include "fe/data/values.h"

namespace fe
{
	class value_scope
	{
		std::optional<value_scope*> parent;

		std::unordered_map<std::string, values::unique_value> variables;

		std::unordered_map<core_ast::identifier, value_scope*> modules;

	public:
		value_scope();
		value_scope(const value_scope& other);
		value_scope(value_scope&& other);

		void add_module(const core_ast::identifier&, value_scope*);

		void set_parent(value_scope* parent);

		void merge(const value_scope& other);

		std::optional<values::value*> valueof(const core_ast::identifier& name, size_t scope_depth);

		void set_value(const std::string& name, values::unique_value value);
		void set_value(const std::string& name, values::unique_value value, std::size_t depth);

		std::string to_string();
	};
}

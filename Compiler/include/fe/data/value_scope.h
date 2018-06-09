#pragma once
#include <optional>
#include <unordered_map>

#include "fe/data/ast_data.h"
#include "fe/data/values.h"

namespace fe
{
	class value_scope
	{
		std::unordered_map<std::string_view, values::unique_value> variables;

		std::unordered_map<core_ast::identifier, scope_index> modules;

		std::optional<scope_index> parent;

	public:
		using get_scope_cb = std::function<value_scope*(scope_index)>;

		value_scope();
		value_scope(const value_scope& other);
		value_scope(value_scope&& other);

		void clear();

		void add_module(const core_ast::identifier&, scope_index);

		void set_parent(scope_index parent);

		void merge(const value_scope& other);

		std::optional<values::value*> valueof(const core_ast::identifier& name, size_t scope_depth, get_scope_cb);

		void set_value(std::string_view name, values::unique_value value);
		void set_value(std::string_view name, values::unique_value value, std::size_t depth, get_scope_cb);

		std::string to_string();
	};
}

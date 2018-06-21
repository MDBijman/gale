#pragma once
#include <optional>
#include <unordered_map>

#include "fe/data/ast_data.h"
#include "fe/data/values.h"
#include "utils/memory/data_store.h"

namespace fe::core_ast
{
	class value_scope
	{
		std::unordered_map<std::string, values::unique_value> variables;

	public:
		scope_index parent;

		value_scope();
		value_scope(const value_scope& other);
		value_scope(value_scope&& other);
		value_scope& operator=(const value_scope& other);
		value_scope& operator=(value_scope&& other);

		void clear();
		void merge(const value_scope& other);

		std::optional<values::value*> valueof(const core_ast::identifier& name);
		void set_value(std::string name, values::unique_value value);

		std::string to_string();
	};

	class stack
	{
		std::unordered_map<std::vector<std::string>, scope_index> modules;

		memory::dynamic_store<value_scope> scopes;

		scope_index ancestor(scope_index s, uint32_t height)
		{
			return height == 0 ? s : ancestor(get_at(s).parent, height - 1);
		}

	public:
		scope_index create() { return scopes.create(); }
		value_scope& get_at(scope_index i) { return scopes.get_at(i); }

		value_scope& get_scope(scope_index s)
		{
			return scopes.get_at(s);
		}

		void add_module(const std::vector<std::string>& module_id, scope_index module_scope)
		{
			modules.insert({ module_id, module_scope });
		}

		/*
		* s is the scope_index from the scope in which the value reading happens. name is the name of 
		* the variable being read. The stack will retrieve the value from the proper scope by using the 
		* scope distance of name.
		*/
		std::optional<values::value*> get_value(scope_index s, const core_ast::identifier& name)
		{
			if (name.modules.size() > 0)
			{
				return scopes
					.get_at(modules.at(name.modules))
					.valueof(name);
			}

			return get_at(ancestor(s, name.scope_distance)).valueof(name.variable_name);
		}
		/*
		* s is the scope_index from the scope in which the value writing happens. name is the name of 
		* the variable being written. The stack will set the value in the proper scope by using the 
		* scope distance of name.
		*/
		void set_value(scope_index s, const core_ast::identifier& name, values::unique_value value)
		{
			if (name.modules.size() > 0)
			{
				scopes
					.get_at(modules.at(name.modules))
					.set_value(core_ast::identifier{ {}, name.variable_name, 0, name.offsets }, std::move(value));

				return;
			}

			return get_at(ancestor(s, name.scope_distance)).set_value(name.variable_name, std::move(value));
		}
	};
}

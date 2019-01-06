#include "fe/data/type_scope.h"
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	void type_scope::merge(type_scope other)
	{
		for (auto&& var : other.variables)
		{
			this->variables.insert(std::move(var));
		}

		for (auto&& type : other.types)
		{
			this->types.insert(std::move(type));
		}
	}

	void type_scope::clear()
	{
		this->variables.clear();
		this->modules.clear();
		this->types.clear();
	}

	void type_scope::add_module(module_name name, scope_index scope)
	{
		this->modules.insert({ name, scope });
	}

	void type_scope::set_parent(scope_index other)
	{
		this->parent = other;
	}

	void type_scope::set_type(name n, types::unique_type t)
	{
		this->variables.insert({ n, std::move(t) });
	}

	std::optional<type_scope::var_lookup> type_scope::resolve_variable(const identifier& id, get_scope_cb cb)
	{
		// Search in parent scope if non-zero scope distance
		if (*id.scope_distance > 0)
		{
			if (!parent)
				throw typecheck_error{ "Scope distance > 0 but no parent scope" };

			identifier new_id = id;
			(*new_id.scope_distance)--;

			if (auto parent_lookup = cb(*parent)->resolve_variable(new_id, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		if (id.module_path.size() > 0)
		{
			auto pos = modules.find(id.module_path);
			if (pos == modules.end())
				return std::nullopt;

			identifier new_id = id;
			new_id.module_path = {};

			if (auto module_lookup = cb(pos->second)->resolve_variable(new_id, cb); module_lookup)
			{
				return module_lookup;
			}
		}

		if (auto pos = variables.find(id.name); pos != variables.end())
		{
			return var_lookup{ std::distance(variables.begin(), pos), *pos->second.get() };
		}

		return std::nullopt;
	}

	void type_scope::define_type(name n, types::unique_type t)
	{
		assert(types.find(n) == types.end());
		this->types.insert({ n, std::move(t) });
	}

	std::optional<type_scope::type_lookup> type_scope::resolve_type(const identifier& id, get_scope_cb cb)
	{
		assert(id.scope_distance);

		// Search in parent scope if non-zero scope distance
		if (*id.scope_distance > 0)
		{
			if (!parent)
				throw typecheck_error{ "Scope distance > 0 but no parent scope" };

			identifier new_id = id;
			(*new_id.scope_distance)--;

			if (auto parent_lookup = cb(*parent)->resolve_type(new_id, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		if (id.module_path.size() > 0)
		{
			auto pos = modules.find(id.module_path);
			if (pos == modules.end())
				return std::nullopt;

			identifier new_id = id;
			new_id.module_path = {};

			if (auto module_lookup = cb(pos->second)->resolve_type(new_id, cb); module_lookup)
			{
				return module_lookup;
			}
		}

		if (auto pos = types.find(id.name); pos != types.end())
		{
			return type_lookup{ std::distance(types.begin(), pos), *pos->second.get() };
		}

		return std::nullopt;
	}
}

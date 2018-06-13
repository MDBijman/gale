#include "fe/data/name_scope.h"
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	void name_scope::merge(name_scope other)
	{
		for (auto&& pair : other.variables)
		{
			this->variables.insert(std::move(pair));
		}

		for (auto&& pair : other.opaque_variables)
		{
			this->opaque_variables.insert(std::move(pair));
		}

		for (auto&& pair : other.types)
		{
			this->types.insert(std::move(pair));
		}

		for (auto&& mod : other.modules)
		{
			this->modules.insert(std::move(mod));
		}
	}

	void name_scope::set_parent(scope_index other)
	{
		this->parent = other;
	}

	size_t name_scope::depth(get_scope_cb cb)
	{
		if (parent) return cb(*parent)->depth(cb) + 1;
		return 0;
	}

	void name_scope::add_module(module_name module_name, scope_index scope)
	{
		this->modules.insert({ module_name, scope });
	}

	void name_scope::declare_variable(name id, node_id type_id)
	{
		assert(opaque_variables.find(id) == opaque_variables.end());
		assert(variables.find(id) == variables.end());
		this->variables.insert({ id, { type_id, false } });
	}

	void name_scope::declare_variable(name id)
	{
		assert(opaque_variables.find(id) == opaque_variables.end());
		assert(variables.find(id) == variables.end());
		this->opaque_variables.insert({ id, false });
	}

	void name_scope::define_variable(name id)
	{
		if (variables.find(id) != variables.end()) variables.at(id).second = true;
		if (opaque_variables.find(id) != opaque_variables.end()) opaque_variables.at(id) = true;
	}

	std::optional<name_scope::var_lookup> name_scope::resolve_variable(module_name module, name var, get_scope_cb cb) const
	{
		if (module.size() == 0) return this->resolve_variable(var, cb);

		if (auto mod_pos = modules.find(module); mod_pos != modules.end())
		{
			return cb(mod_pos->second)->resolve_variable(var, cb);
		}
		// If all lookups fail try parent
		else if (parent)
		{
			if (auto parent_lookup = cb(*parent)->resolve_variable(module, var, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;
	}

	std::optional<name_scope::var_lookup> name_scope::resolve_variable(name var, get_scope_cb cb) const
	{
		if (auto pos = variables.find(var); pos != variables.end())
		{
			return var_lookup{ 0, pos->second.first };
		}
		else if (auto pos = opaque_variables.find(var); pos != opaque_variables.end())
		{
			return var_lookup{ 0, std::nullopt };
		}
		else if (parent)
		{
			if (auto parent_lookup = cb(*parent)->resolve_variable(var, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;
	}

	void name_scope::define_type(name n, node_id t)
	{
		this->types.insert({ n, t });
	}

	std::optional<name_scope::type_lookup> name_scope::resolve_type(module_name module, name var, get_scope_cb cb) const
	{
		if (module.size() == 0) return this->resolve_type(var, cb);

		if (auto mod_pos = modules.find(module); mod_pos != modules.end())
		{
			return cb(mod_pos->second)->resolve_type(var, cb);
		}
		else if (parent)
		{
			if (auto parent_lookup = cb(*parent)->resolve_type(module, var, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}
		return std::nullopt;
	}

	std::optional<name_scope::type_lookup> name_scope::resolve_type(name var, get_scope_cb cb) const
	{
		if (auto pos = types.find(var); pos != types.end())
		{
			return type_lookup{ 0, pos->second };
		}
		else if (parent)
		{
			if (auto parent_lookup = cb(*parent)->resolve_type(var, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;
	}
}
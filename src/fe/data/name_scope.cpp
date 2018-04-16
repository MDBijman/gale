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

	void name_scope::set_parent(name_scope* other)
	{
		this->parent = other;
	}

	void name_scope::add_module(const identifier& module_name, name_scope* scope)
	{
		this->modules.insert({ module_name, scope });
	}

	void name_scope::declare_variable(const name& id, node_id type_id)
	{
		assert(opaque_variables.find(id) == opaque_variables.end());
		assert(variables.find(id) == variables.end());
		this->variables.insert({ id, { type_id, false } });
	}

	void name_scope::declare_variable(const name& id)
	{
		assert(opaque_variables.find(id) == opaque_variables.end());
		assert(variables.find(id) == variables.end());
		this->opaque_variables.insert({ id, false });
	}

	void name_scope::define_variable(const name& id)
	{
		if (variables.find(id) != variables.end()) variables.at(id).second = true;
		if (opaque_variables.find(id) != opaque_variables.end()) opaque_variables.at(id) = true;
	}

	std::optional<name_scope::var_lookup> name_scope::resolve_variable(const identifier& module, const name& var) const
	{
		if (module.segments.size() == 0) return this->resolve_variable(var);

		if (auto mod_pos = modules.find(module); mod_pos != modules.end())
		{
			return mod_pos->second->resolve_variable(var);
		}
		// If all lookups fail try parent
		else if (parent)
		{
			if (auto parent_lookup = (*parent)->resolve_variable(module, var); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;

		//{
		//	// Module lookup
		//	for (auto i = 0; i < id.segments.size() - 1; i++)
		//	{
		//		identifier module_id;
		//		module_id.segments = std::vector<std::string>(id.segments.begin(), id.segments.begin() + i + 1);

		//		if (auto pos = modules.find(module_id); pos != modules.end())
		//		{
		//			identifier new_id;
		//			new_id.segments = std::vector<std::string>(id.segments.begin() + i + 1, id.segments.end());

		//			if (auto module_lookup = pos->second->resolve_variable(new_id); module_lookup)
		//				return module_lookup;
		//		}
		//	}
		//}
	}

	std::optional<name_scope::var_lookup> name_scope::resolve_variable(const name& var) const
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
			if (auto parent_lookup = (*parent)->resolve_variable(var); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;
	}

	void name_scope::define_type(const name& n, node_id t)
	{
		this->types.insert({ n, t });
	}

	std::optional<name_scope::type_lookup> name_scope::resolve_type(const identifier& module, const name& var) const
	{
		if (module.segments.size() == 0) return this->resolve_type(var);

		if (auto mod_pos = modules.find(module); mod_pos != modules.end())
		{
			return mod_pos->second->resolve_type(var);
		}
		else if (parent)
		{
			if (auto parent_lookup = (*parent)->resolve_type(module, var); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}
		return std::nullopt;

		/*	for (auto i = 1; i < id.segments.size(); i++)
				{
					identifier module_id;
					module_id.segments = std::vector<std::string>(id.segments.begin(), id.segments.begin() + i);

					if (auto pos = modules.find(module_id); pos != modules.end())
					{
						identifier new_id;
						new_id.segments = std::vector<std::string>(id.segments.begin() + i, id.segments.end());

						if (auto module_lookup = pos->second->resolve_type(new_id); module_lookup)
							return module_lookup;
					}
				}*/
	}

	std::optional<name_scope::type_lookup> name_scope::resolve_type(const name& var) const
	{
		if (auto pos = types.find(var); pos != types.end())
		{
			return type_lookup{ 0, pos->second };
		}
		else if (parent)
		{
			if (auto parent_lookup = (*parent)->resolve_type(var); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
		}

		return std::nullopt;
	}
}
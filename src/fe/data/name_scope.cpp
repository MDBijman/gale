#include "fe/data/name_scope.h"
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	field::field(name n, std::vector<field> fields) : id(n), fields(fields) {}

	std::optional<std::vector<int64_t>> field::resolve(const identifier& id) const
	{
		if (id.segments.size() == 0)
			return std::nullopt;

		if (id.segments.size() == 1)
		{
			if (this->id == id.segments[0]) return std::vector<int64_t>();
			else return std::nullopt;
		}

		auto next_id = id.without_first_segment();
		for (auto i = 0; i < fields.size(); i++)
		{
			auto offsets = fields[i].resolve(next_id);
			if (offsets.has_value())
			{
				auto new_offsets = offsets.value();
				new_offsets.insert(new_offsets.begin(), i);
				return new_offsets;
			}
		}

		return std::nullopt;
	}

	void name_scope::merge(name_scope other)
	{
		for (auto&& pair : other.variables)
		{
			this->variables.insert(std::move(pair));
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

	void name_scope::declare_variable(const name& id)
	{
		this->variables.insert({ id, false });
	}

	void name_scope::define_variable(const name& id)
	{
		this->variables.at(id) = true;
	}

	std::optional<name_scope::var_lookup> name_scope::resolve_variable(const identifier& id) const
	{
		if (auto pos = variables.find(id.segments[0]); pos != variables.end())
		{
			return var_lookup{ 0 };
		}
		else 
		{
			for(auto i = 0; i < id.segments.size() - 1; i++)
			{
				identifier module_id;
				module_id.segments = std::vector<std::string>(id.segments.begin(), id.segments.begin() + i + 1);

				if (auto pos = modules.find(module_id); pos != modules.end())
				{
					identifier new_id;
					new_id.segments = std::vector<std::string>(id.segments.begin() + i + 1, id.segments.end());

					if (auto module_lookup = pos->second->resolve_variable(new_id); module_lookup)
						return module_lookup;
				}
			}

			// If module lookup fails
			if (parent)
			{
				if (auto parent_lookup = (*parent)->resolve_variable(id); parent_lookup)
				{
					parent_lookup->scope_distance++;
					return parent_lookup;
				}
			}
		}

		return std::nullopt;
	}

	void name_scope::define_type(const name& n, const std::vector<field>& t)
	{
		this->types.insert({ n, field(n, t) });
	}

	std::optional<name_scope::type_lookup> name_scope::resolve_type(const identifier& id) const
	{
		if (auto pos = types.find(id.segments[0]); pos != types.end())
		{
			return type_lookup{ 0, pos->second };
		}
		else
		{
			for (auto i = 1; i < id.segments.size(); i++)
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
			}

			if (parent)
			{
				if (auto parent_lookup = (*parent)->resolve_type(id); parent_lookup)
				{
					parent_lookup->scope_distance++;
					return parent_lookup;
				}
			}
		}

		return std::nullopt;
	}
}
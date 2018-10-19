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

	types::type& type_scope::resolve_offsets(const std::vector<size_t>& offsets, types::type* t, size_t cur)
	{
		if (cur < offsets.size())
		{
			if (types::product_type* as_product = dynamic_cast<types::product_type*>(t))
			{
				return resolve_offsets(offsets, as_product->product.at(offsets.at(cur)).get(), cur + 1);
			}
			else
			{
				throw typecheck_error{ "Cannot access field in non-product type" };
			}
		}
		else
		{
			return *t;
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
		if (auto pos = variables.find(id.segments[0]); pos != variables.end())
		{
			return var_lookup{ std::distance(variables.begin(), pos), resolve_offsets(id.offsets, pos->second.get()) };
		}

		for (auto i = 0; i < id.segments.size() - 1; i++)
		{
			auto module_name = std::vector<std::string>(id.segments.begin(), id.segments.begin() + i + 1);

			auto pos = modules.find(module_name);
			if (pos == modules.end()) continue;

			identifier new_id;
			new_id.segments = std::vector<std::string>(id.segments.begin() + i + 1, id.segments.end());

			if (auto module_lookup = cb(pos->second)->resolve_variable(new_id, cb); module_lookup)
				return module_lookup;
		}

		if (parent)
		{
			if (auto parent_lookup = cb(*parent)->resolve_variable(id, cb); parent_lookup)
			{
				parent_lookup->scope_distance++;
				return parent_lookup;
			}
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

			auto parent_lookup = cb(*parent)->resolve_type(id, cb);

			if (!parent_lookup) 
				return std::nullopt;

			parent_lookup->scope_distance++;
			return parent_lookup;
		}

		// Search in this scope
		if (auto pos = types.find(id.segments[0]); pos != types.end())
		{
			return type_lookup{ std::distance(types.begin(), pos), *pos->second };
		}

		// Search in modules
		for (auto i = 1; i < id.segments.size(); i++)
		{
			auto module_name = std::vector<std::string>(id.segments.begin(), id.segments.begin() + i);

			if (auto pos = modules.find(module_name); pos != modules.end())
			{
				identifier new_id;
				new_id.scope_distance = 0;
				new_id.segments = std::vector<std::string>(id.segments.begin() + i, id.segments.end());

				if (auto module_lookup = cb(pos->second)->resolve_type(new_id, cb); module_lookup)
					return module_lookup;
			}
		}

		return std::nullopt;
	}
}

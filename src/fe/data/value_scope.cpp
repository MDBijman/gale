#include "fe/data/value_scope.h"
#include "fe/data/values.h"

namespace fe
{
	value_scope::value_scope() {}
	value_scope::value_scope(const value_scope& other) : parent(other.parent), modules(other.modules)
	{
		for (const auto& elem : other.variables)
			this->variables.insert({ elem.first, values::unique_value(elem.second->copy()) });
	}
	value_scope::value_scope(value_scope&& other) : 
		parent(other.parent), modules(std::move(other.modules)), variables(std::move(other.variables)) {}

	void value_scope::add_module(const core_ast::identifier& id, scope_index o)
	{
		this->modules.insert({ id, o });
	}

	void value_scope::set_parent(scope_index parent)
	{
		this->parent = parent;
	}

	void value_scope::merge(const value_scope& other)
	{
		for (const auto& elem : other.variables)
			this->variables.insert({ elem.first, values::unique_value(elem.second->copy()) });
	}

	std::optional<values::value*> value_scope::valueof(const core_ast::identifier& name, size_t scope_depth, get_scope_cb cb)
	{
		// Check parent scope
		if (scope_depth > 0)
		{
			return parent ? cb(*parent)->valueof(name, scope_depth - 1, cb) : std::nullopt;
		}

		// Check modules
		if (name.modules.size() > 0)
		{
			std::vector<std::string> modules(name.modules.begin(), name.modules.end() - 1);
			auto& module = this->modules.at(core_ast::identifier(modules, name.modules.back(), 0, {}));
			return cb(module)->valueof(core_ast::identifier(name.variable_name), 0, cb);
		}

		// Check this scope
		if (auto loc = variables.find(name.variable_name); loc != variables.end())
		{
			values::value* value = loc->second.get();

			for (auto offset : name.offsets)
			{
				value = dynamic_cast<values::tuple*>(value)->val.at(offset).get();
			}

			return value;
		}

		return std::nullopt;
	}

	void value_scope::set_value(const std::string& name, values::unique_value value)
	{
		this->variables.insert({ name, std::move(value) });
	}

	void value_scope::set_value(const std::string& name, values::unique_value value, std::size_t depth, get_scope_cb cb)
	{
		if (depth > 0)
		{
			cb(*parent)->set_value(name, std::move(value), depth - 1, cb);
		}
		else
		{
			this->variables.insert_or_assign(name, std::move(value));
		}
	}

	std::string value_scope::to_string()
	{
		std::string r;
		for (const auto& pair : variables)
		{
			std::string t = "\n\t" + pair.first + ": ";
			t.append(pair.second->to_string());
			r.append(t);
			r.append(",");
		}
		return r;
	}
}
#include "fe/data/value_scope.h"
#include "fe/data/values.h"

namespace fe::core_ast
{
	value_scope::value_scope() {}
	value_scope::value_scope(const value_scope& other) : parent(other.parent)
	{
		for (const auto& pair : other.variables)
			this->variables.emplace(pair.first, values::unique_value(pair.second->copy()));
	}
	value_scope::value_scope(value_scope&& other) : 
		parent(other.parent), variables(std::move(other.variables)) {}

	value_scope& value_scope::operator=(const value_scope& other)
	{
		this->variables.clear();
		for (const auto& pair : other.variables)
			this->variables.emplace(pair.first, values::unique_value(pair.second->copy()));

		this->parent = other.parent;
		return *this;
	}

	value_scope& value_scope::operator=(value_scope&& other)
	{
		this->variables = std::move(other.variables);
		this->parent = other.parent;
		return *this;
	}

	void value_scope::clear()
	{
		this->variables.clear();
	}

	void value_scope::merge(const value_scope& other)
	{
		for (const auto& pair : other.variables)
			this->variables.emplace(pair.first, values::unique_value(pair.second->copy()));
	}

	std::optional<values::value*> value_scope::valueof(const core_ast::identifier& name)
	{
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

	void value_scope::set_value(std::string name, values::unique_value value)
	{
		this->variables.insert({ name, std::move(value) });
	}

	std::string value_scope::to_string()
	{
		std::string r;
		for (const auto& pair : variables)
		{
			std::string t = "\n\t" + std::string(pair.first) + ": ";
			t.append(pair.second->to_string());
			r.append(t);
			r.append(",");
		}
		return r;
	}
}
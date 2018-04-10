#pragma once
#include "fe/data/values.h"
#include "fe/data/core_ast.h"

#include <unordered_map>
#include <regex>
#include <assert.h>
#include <string>

namespace fe::detail
{
	class value_scope
	{
	public:
		value_scope() {}
		value_scope(const value_scope& other) : parent(other.parent)
		{
			for (const auto& elem : other.variables)
				this->variables.insert({ elem.first, values::unique_value(elem.second->copy()) });
		}

		void merge(const value_scope& other)
		{
			for (const auto& elem : other.variables)
				this->variables.insert({ elem.first, values::unique_value(elem.second->copy()) });
		}

		std::optional<values::value*> valueof(const core_ast::identifier& name,
			size_t scope_depth)
		{
			if (scope_depth > 0)
			{
				return parent.has_value()
					? parent.value()->valueof(name, scope_depth - 1)
					: std::nullopt;
			}

			auto loc = variables.find(name.variable_name);
			if (loc == variables.end())
				return std::nullopt;

			values::value* value = loc->second.get();

			for (auto offset : name.offsets)
			{
				value = dynamic_cast<values::tuple*>(value)->val.at(offset).get();
			}

			return value;
		}

		void set_value(const std::string& name, values::unique_value value)
		{
			this->variables.insert({ name, std::move(value) });
		}

		void set_value(const std::string& name, values::unique_value value, std::size_t depth)
		{
			if (depth > 0)
			{
				parent.value()->set_value(name, std::move(value), depth - 1);
			}
			else
			{
				this->variables.insert_or_assign(name, std::move(value));
			}
		}

		void set_parent(std::shared_ptr<value_scope> parent)
		{
			this->parent = parent;
		}

		std::string to_string()
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

	private:
		std::optional<std::shared_ptr<value_scope>> parent;

		std::unordered_map<std::string, values::unique_value> variables;
	};
}

namespace fe
{
	class runtime_environment
	{
	public:
		runtime_environment() { push(); }
		runtime_environment(const runtime_environment& other, std::size_t depth) :
			scopes(other.scopes),
			modules(other.modules)
		{
			for (std::size_t i = 0; i < depth; i++) pop();
		}

		void push()
		{
			scopes.push_back(std::make_shared<detail::value_scope>());

			if (scopes.size() > 1)
			{
				auto& parent = scopes.at(scopes.size() - 2);
				scopes.back()->set_parent(parent);
			}
		}

		void pop()
		{
			scopes.pop_back();
		}

		void add_global_module(runtime_environment&& other)
		{
			this->scopes.front()->merge(std::move(*other.scopes.front()));
		}

		void add_module(std::vector<std::string> name, runtime_environment other)
		{
			if (name.size() == 0)
			{
				add_global_module(std::move(other));
			}
			else
			{
				if (auto loc = modules.find(name[0]); loc != modules.end())
				{
					loc->second.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));
				}
				else
				{
					runtime_environment new_re;

					new_re.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));

					modules.insert({ std::move(name[0]), std::move(new_re) });
				}
			}
		}

		void add_module(std::string name, runtime_environment other)
		{
			add_module(std::vector<std::string>{ std::move(name) }, std::move(other));
		}

		runtime_environment get_module(std::string name)
		{
			return modules.find(name)->second;
		}

		void set_value(const std::string& name, values::unique_value value)
		{
			assert(name.size() >= 1);
			this->scopes.back()->set_value(name, std::move(value));
		}

		void set_value(const std::string& name, const values::value& value)
		{
			assert(name.size() >= 1);
			this->scopes.back()->set_value(name, values::unique_value(value.copy()));
		}

		void set_value(const std::string& name, const values::value& value, std::size_t scope_depth)
		{
			assert(name.size() >= 1);
			this->scopes.back()->set_value(name, values::unique_value(value.copy()), scope_depth);
		}

		std::optional<values::value*> valueof(const core_ast::identifier& identifier) const
		{
			if (identifier.modules.size() > 0)
			{
				auto new_id = identifier.without_first_module();
				new_id.scope_depth = 0;
				return modules.find(identifier.modules.at(0))->second.valueof(std::move(new_id));
			}
			else
			{
				return this->scopes.back()->valueof(identifier, identifier.scope_depth);
			}
		}

		std::string to_string(bool include_modules = true)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};

			std::string r = "runtime_environment (";

			for (auto& scope : scopes)
			{
				r.append(scope->to_string());
			}

			if (include_modules)
			{
				r.append(indent("\n" + std::string("modules (")));
				for (auto& string_module_pair : modules)
				{
					r.append(indent(indent(
						"\n" + string_module_pair.second.to_string(false) + ","
					)));
				}
				r.append("\n\t)");
			}

			r.append("\n)");
			return r;
		}

	private:
		std::vector<std::shared_ptr<detail::value_scope>> scopes;
		std::unordered_multimap<std::string, runtime_environment> modules;
	};
}
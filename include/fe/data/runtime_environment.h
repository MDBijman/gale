#pragma once
#include "fe/data/values.h"
#include "fe/data/core_ast.h"

#include <unordered_map>
#include <regex>
#include <assert.h>
#include <string>

namespace fe
{
	class runtime_environment
	{
	public:
		runtime_environment() {}
		runtime_environment(std::unordered_map<std::string, values::unique_value> values) : values(std::move(values)) {}
		runtime_environment(const runtime_environment& other)
		{
			for (decltype(auto) pair : other.values)
			{
				values.emplace(pair.first, values::unique_value(pair.second->copy()));
			}

			for (decltype(auto) pair : other.modules)
			{
				modules.insert({ pair.first, pair.second });
			}
		}

		runtime_environment& operator=(const runtime_environment& other)
		{
			for (decltype(auto) pair : other.values)
			{
				values.emplace(pair.first, values::unique_value(pair.second->copy()));
			}

			for (decltype(auto) pair : other.modules)
			{
				modules.insert({ pair.first, pair.second });
			}
			return *this;
		}

		runtime_environment& operator=(runtime_environment&& other)
		{
			this->values = std::move(other.values);
			this->modules = std::move(other.modules);
			return *this;
		}

		void merge(runtime_environment&& other)
		{
			values.insert(std::make_move_iterator(other.values.begin()), std::make_move_iterator(other.values.end()));

			for (auto&& other_module : other.modules)
			{
				this->add_module(other_module.first, std::move(other_module.second));
			}
		}

		void add_global_module(runtime_environment&& other)
		{
			this->merge(std::move(other));
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
			add_module({ std::move(name) }, std::move(other));
		}

		runtime_environment get_module(std::string name)
		{
			return modules.find(name)->second;
		}

		void set_value(const std::string& name, values::unique_value value)
		{
			if (name == "")
				std::cout << "!\n";
			values.insert_or_assign(name, std::move(value));
		}

		void set_value(const std::string& name, const values::value& value)
		{
			if (name == "")
				std::cout << "!\n";
			values.insert_or_assign(name, values::unique_value(value.copy()));
		}

		values::unique_value valueof(const core_ast::identifier& identifier) const
		{
			if (identifier.modules.size() > 0)
			{
				return modules.find(identifier.modules.at(0))->second.valueof(identifier.without_first_module());
			}
			else
			{
				const values::value* value = values.at(identifier.variable_name).get();

				for (int i = 0; i < identifier.offsets.size(); i++)
				{
					const auto& product_value = dynamic_cast<const values::tuple*>(value);

					value = product_value->content.at(identifier.offsets.at(i)).get();
				}

				return values::unique_value(value->copy());
			}
		}

		std::string to_string(bool include_modules = true)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};

			std::string r = "runtime_environment (";

			for (auto& pair : values)
			{
				std::string t = "\n\t" + pair.first + ": ";
				t.append(pair.second->to_string());
				r.append(t);
				r.append(",");
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
		std::unordered_map<std::string, values::unique_value> values;
		std::unordered_multimap<std::string, runtime_environment> modules;
	};
}
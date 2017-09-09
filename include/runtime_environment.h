#pragma once
#include "values.h"
#include <unordered_map>
#include <regex>
#include <string>

namespace fe
{
	class runtime_environment
	{
	public:
		runtime_environment() {}
		runtime_environment(std::unordered_map<std::string, values::value> values) : values(values) {}
		runtime_environment(const runtime_environment& other) : values(other.values), modules(other.modules), name(other.name) {}

		void add_module(runtime_environment&& other)
		{
			// TODO fix issue of setting the module name after a module (with that name) -> modules are not merged
			if (other.name.has_value() && other.name.value() != this->name)
			{
				modules.insert({ other.name.value(), other });
			}
			else
			{
				values.insert(std::make_move_iterator(other.values.begin()), std::make_move_iterator(other.values.end()));
			}
		}

		runtime_environment get_module(std::string name)
		{
			return modules.find(name)->second;
		}

		void set_value(const std::string& name, values::value&& value)
		{
			values.insert_or_assign(name, value);
		}

		const values::value& valueof(const std::string& name) const
		{
			return values.at(name);
		}

		const values::value& valueof(const std::vector<std::string>& identifier) const
		{
			if (identifier.size() == 1)
				return valueof(identifier.at(0));
			else return modules.find(identifier.at(0))->second.valueof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
		}

		std::string to_string(bool include_modules = true)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};

			std::string r = name.has_value() ? 
				"runtime_environment: " + name.value() + " (" : 
				"runtime_environment (";

			for (auto& pair : values)
			{
				std::string t = "\n\t" + pair.first + ": ";
				t.append(std::visit(values::to_string, pair.second));
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

		std::optional<std::string> name;

	private:
		std::unordered_map<std::string, values::value> values;
		std::unordered_multimap<std::string, runtime_environment> modules;
	};
}
#pragma once
#include "values.h"
#include "core_ast.h"
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
			// TODO fix issue of setting the module name after a module (with that name) -> namespaces are not merged
			if (other.name.has_value() && other.name.value() != this->name)
			{
				auto existing_module_location = modules.find(other.name.value());

				if (existing_module_location != modules.end())
					// Merge module with the existing one
				{
					other.name = std::optional<std::string>();
					existing_module_location->second.add_module(std::move(other));
				}
				else
				{
					modules.insert({ other.name.value(), other });
				}
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

		const values::value& valueof(const core_ast::identifier& identifier) const
		{
			if (identifier.modules.size() > 0)
			{
				return modules.find(identifier.modules.at(0))->second.valueof(identifier.without_first_module());
			}
			else
			{
				std::reference_wrapper<const values::value> value = values.at(identifier.variable_name);

				for (int i = 0; i < identifier.offsets.size(); i++)
				{
					auto& product_value = std::get<values::tuple>(value.get());

					value = product_value.content.at(identifier.offsets.at(i));
				}

				return value;
			}
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
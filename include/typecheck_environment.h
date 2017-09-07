#pragma once
#include "types.h"
#include <unordered_map>
#include <string>

namespace fe
{
	class typecheck_environment
	{
	public:
		typecheck_environment() {}
		typecheck_environment(std::unordered_map<std::string, types::type> type_mapping) : types(type_mapping) {}
		typecheck_environment(const typecheck_environment& other) : types(other.types), name(other.name), modules(other.modules) {}

		void add_module(typecheck_environment&& other)
		{
			// TODO fix issue of setting the module name after a module (with that name) -> modules are not merged
			if (other.name.has_value() && other.name.value() != this->name)
			{
				modules.insert({ other.name.value(), other });
			}
			else
			{
				types.insert(std::make_move_iterator(other.types.begin()), std::make_move_iterator(other.types.end()));
			}
		}

		void set_type(const std::string& name, types::type type)
		{
			types.insert_or_assign(name, type);
		}

		const types::type& typeof(const std::string& name) const
		{
			return types.at(name);
		}

		const types::type& typeof(const std::vector<std::string>& identifier) const
		{
			if (identifier.size() == 1)
				return typeof(identifier.at(0));
			else return modules.find(identifier.at(0))->second.typeof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
		}

		std::string to_string(bool include_modules = false)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};
			
			std::string r("type_environment (");

			uint32_t counter = 0;
			for (auto& pair : types)
			{
				std::string t = "\n\t" + pair.first + ": ";
				t.append(std::visit(types::to_string, pair.second));
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
		std::unordered_map<std::string, types::type> types;
		std::unordered_multimap<std::string, typecheck_environment> modules;
	};
}

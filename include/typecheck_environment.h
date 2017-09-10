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
		typecheck_environment(const typecheck_environment& other) : types(other.types), name(other.name), namespaces(other.namespaces) {}

		void add_module(typecheck_environment&& other)
		{
			// TODO fix issue of setting the module name after a module (with that name) -> namespaces are not merged
			if (other.name.has_value() && other.name.value() != this->name)
			{
				auto existing_namespace_location = namespaces.find(other.name.value());

				if (existing_namespace_location != namespaces.end())
					// Merge module with the existing one
				{
					other.name = std::optional<std::string>();
					existing_namespace_location->second.add_module(std::move(other));
				}
				else
				{
					namespaces.insert({ other.name.value(), other });
				}
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
			else
			{
				auto location = namespaces.find(identifier.at(0));
				if (location != namespaces.end())
					return location->second.typeof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
				else
				{
					auto& product_type = std::get<types::product_type>(
						typeof(std::get<types::name_type>(
							types.at(identifier.at(0))
						).name)
					);
					auto& nested_type = std::find_if(product_type.product.begin(), product_type.product.end(), [&identifier](auto& x) { return x.first == identifier.at(1); });

					return nested_type->second;
				}

			}
		}

		std::string to_string(bool include_modules = false)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};
			
			std::string r = name.has_value() ? 
				"type_environment: " + name.value() + " (" : 
				"type_environment (";

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
				for (auto& string_namespace_pair : namespaces)
				{
					r.append(indent(indent(
						"\n" + string_namespace_pair.second.to_string(false) + ","
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
		std::unordered_multimap<std::string, typecheck_environment> namespaces;
	};
}

#pragma once
#include "types.h"
#include "extended_ast.h"
#include "error.h"
#include <unordered_map>
#include <string>

namespace fe
{
	class typecheck_environment
	{
	public:
		typecheck_environment() {}
		typecheck_environment(std::optional<std::string> name) : name(name) {}
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

		void set_type(const std::string& id, types::type type)
		{
			types.insert_or_assign(id, type);
		}

		void set_type(const extended_ast::identifier& id, types::type type)
		{
			if (id.segments.size() == 1)
			{
				types.insert_or_assign(id.segments.at(0), type);
			}
			else
			{
				namespaces.find(id.segments.at(0))->second.set_type(id.without_first_segment(), type);
			}
		}

		std::variant<std::reference_wrapper<const types::type>, type_env_error> typeof(const extended_ast::identifier& id) const
		{
			if (id.segments.size() == 1)
			{
				if (types.find(id.segments.at(0)) == types.end())
					return type_env_error{ "Identifier " + id.to_string() + " is unbounded" };

				return std::ref(types.at(id.segments.at(0)));
			}
			else
			{
				const types::type* type = nullptr;

				if (namespaces.find(id.segments.at(0)) != namespaces.end())
				{
					return namespaces
						.find(id.segments.at(0))->second
						.typeof(id.without_first_segment());
				}

				type = &types.at(id.segments.at(0));

				for (int i = 1; i < id.segments.size(); i++)
				{
					auto& product_type = std::get<types::product_type>(*type);
					auto type_location = std::find_if(product_type.product.begin(), product_type.product.end(), [&](const std::pair<std::string, types::type>& x) {
						return x.first == id.segments.at(i);
					});

					type = &type_location->second;
				}

				return *type;
			}
		}

		extended_ast::identifier build_access_pattern(extended_ast::identifier&& id, int index = 0) const
		{
			if (namespaces.find(id.segments.at(index)) != namespaces.end())
			{
				return namespaces.find(id.segments.at(index))->second.build_access_pattern(std::move(id), index + 1);
			}
			else
			{
				auto variable_name = id.segments.at(index);

				std::reference_wrapper<const types::type> current_type = types.at(variable_name);
				for (int i = index + 1; i < id.segments.size(); i++)
				{
					// Find the segment within the product type that has the correct name
					auto& product_type = std::get<types::product_type>(current_type.get());
					auto next_loc = std::find_if(product_type.product.begin(), product_type.product.end(), [&](auto& x) {
						return x.first == id.segments.at(i);
					});

					// Calculate offset
					auto offset = std::distance(product_type.product.begin(), next_loc);
					id.offsets.push_back(offset);
					current_type = product_type.product.at(offset).second;
				}
			}

			return id;
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

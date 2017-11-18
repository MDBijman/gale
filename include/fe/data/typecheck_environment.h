#pragma once
#include "fe/data/types.h"
#include "fe/data/extended_ast.h"
#include "fe/pipeline/error.h"

#include <unordered_map>
#include <string>
#include <regex>

namespace fe
{
	class typecheck_environment
	{
	public:
		typecheck_environment() {}
		typecheck_environment(std::optional<std::string> name) : name(name) {}
		typecheck_environment(std::unordered_map<std::string, types::unique_type> type_mapping) : types(std::move(type_mapping)) {}
		typecheck_environment(const typecheck_environment& other) : name(other.name) 
		{
			for (decltype(auto) pair : other.types)
			{
				types.emplace(pair.first, types::unique_type(pair.second->copy()));
			}

			for (decltype(auto) pair : other.namespaces)
			{
				namespaces.insert({ pair.first, pair.second });
			}
		}

		typecheck_environment& operator=(const typecheck_environment& other)
		{
			for (decltype(auto) pair : other.types)
			{
				types.emplace(pair.first, types::unique_type(pair.second->copy()));
			}

			for (decltype(auto) pair : other.namespaces)
			{
				namespaces.insert({ pair.first, pair.second });
			}
			this->name = other.name;
			return *this;
		}

		typecheck_environment& operator=(typecheck_environment&& other)
		{
			this->types = std::move(other.types);
			this->namespaces = std::move(other.namespaces);
			this->name = std::move(other.name);
			return *this;
		}

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
					namespaces.insert({ other.name.value(), std::move(other) });
				}
			}
			else
			{
				types.insert(std::make_move_iterator(other.types.begin()), std::make_move_iterator(other.types.end()));

				for (auto&& other_namespaces : other.namespaces)
				{
					this->add_module(std::move(other_namespaces.second));
				}
			}
		}

		void set_type(const std::string& id, types::unique_type type)
		{
			types.emplace(id, std::move(type));
		}

		void set_type(const std::string& id, const types::type& type)
		{
			types.emplace(id, types::unique_type(type.copy()));
		}

		void set_type(const extended_ast::identifier& id, types::unique_type type)
		{
			if (id.segments.size() == 1)
			{
				types.emplace(id.segments.at(0), std::move(type));
			}
			else
			{
				namespaces.find(id.segments.at(0))->second.set_type(id.without_first_segment(), std::move(type));
			}
		}

		std::variant<std::reference_wrapper<types::type>, type_env_error> typeof(const std::string& id) const
		{
			return typeof(extended_ast::identifier{ {id} });
		}

		std::variant<std::reference_wrapper<types::type>, type_env_error> typeof(const extended_ast::identifier& id) const
		{
			if (id.segments.size() == 1 && id.segments.at(0) == "_")
				return type_env_error{"Cannot use identifier _"};

			if (id.segments.size() == 1)
			{
				if (types.find(id.segments.at(0)) == types.end())
					return type_env_error{ "Identifier " + id.to_string() + " is unbounded" };

				return *types.at(id.segments.at(0));
			}
			else
			{
				if (namespaces.find(id.segments.at(0)) != namespaces.end())
				{
					return namespaces
						.find(id.segments.at(0))->second
						.typeof(id.without_first_segment());
				}

				std::reference_wrapper<types::type> type = *types.at(id.segments.at(0));

				for (int i = 1; i < id.segments.size(); i++)
				{
					const auto product_type = dynamic_cast<const types::product_type*>(&type.get());
					auto type_location = std::find_if(product_type->product.begin(), product_type->product.end(), [&](const auto& x) {
						return x.first == id.segments.at(i);
					});

					type = *type_location->second;
				}

				return type;
			}
		}

		void build_access_pattern(extended_ast::identifier& id, int index = 0)
		{
			if (namespaces.find(id.segments.at(index)) != namespaces.end())
			{
				namespaces.find(id.segments.at(index))->second.build_access_pattern(id, index + 1);
			}
			else
			{
				auto variable_name = id.segments.at(index);

				auto current_type = types.at(variable_name).get();
				for (int i = index + 1; i < id.segments.size(); i++)
				{
					// Find the segment within the product type that has the correct name
					const auto product_type = dynamic_cast<const types::product_type*>(current_type);
					auto next_loc = std::find_if(product_type->product.begin(), product_type->product.end(), [&](auto& x) {
						return x.first == id.segments.at(i);
					});

					// Calculate offset
					auto offset = std::distance(product_type->product.begin(), next_loc);
					id.offsets.push_back(offset);
					current_type = product_type->product.at(offset).second.get();
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
			for (decltype(auto) pair : types)
			{
				std::string t = "\n\t" + pair.first + ": ";
				t.append(pair.second->to_string());
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
		std::unordered_map<std::string, types::unique_type> types;
		std::unordered_multimap<std::string, typecheck_environment> namespaces;
	};
}

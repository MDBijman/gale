#pragma once
#include "fe/data/types.h"
#include "fe/data/extended_ast.h"
#include "fe/pipeline/error.h"

#include <unordered_map>
#include <string>
#include <regex>


namespace fe::detail
{
	class type_scope
	{
	public:
		type_scope() {}
		type_scope(const type_scope& other) : parent(other.parent)
		{
			for (const auto& elem : other.types)
				this->types.insert({ elem.first, types::unique_type(elem.second->copy()) });
			for (const auto& elem : other.variables)
				this->variables.insert({ elem.first, types::unique_type(elem.second->copy()) });
		}

		void merge(const type_scope& other)
		{
			for (const auto& elem : other.types)
				this->types.insert({ elem.first, types::unique_type(elem.second->copy()) });
			for (const auto& elem : other.variables)
				this->variables.insert({ elem.first, types::unique_type(elem.second->copy()) });
		}

#pragma optimize("", off)
		std::optional<std::reference_wrapper<types::type>> typeof(const extended_ast::identifier& name,
			int scope_depth)
		{
			if (scope_depth > 0)
			{
				if (parent.has_value())
					return parent.value()->typeof(name, scope_depth - 1);
				else
					return std::nullopt;
			}

			auto loc = variables.find(name.segments.front());
			if (loc == variables.end())
				return std::nullopt;

			types::type* type = loc->second.get();

			for (auto offset : name.offsets)
			{
				type = dynamic_cast<types::product_type*>(type)->product.at(offset).get();
			}

			return *type;
		}

		void set_type(const extended_ast::identifier& name, types::unique_type type)
		{
			this->variables.insert({ name.segments.back(), std::move(type) });
		}

		std::optional<std::reference_wrapper<types::type>> resolve_type(const extended_ast::identifier& name)
		{
			auto map_location = this->types.find(name.segments.back());

			if (map_location == this->types.end())
				if (parent.has_value())
					return parent.value()->resolve_type(name);
				else
					return std::nullopt;

			return *map_location->second;
		}

		void define_type(const extended_ast::identifier& name, types::unique_type type)
		{
			this->types.insert({ name.segments.back(), std::move(type) });
		}

		void set_parent(std::shared_ptr<type_scope> parent)
		{
			this->parent = parent;
		}

	private:
		std::optional<std::shared_ptr<type_scope>> parent;

		std::unordered_map<std::string, types::unique_type> variables;
		std::unordered_map<std::string, types::unique_type> types;
	};
}

namespace fe
{
	class type_environment
	{
	public:
		type_environment() { push(); }

		void push()
		{
			scopes.push_back(std::make_shared<detail::type_scope>());

			// There is a parent
			if (scopes.size() > 1) {
				auto& parent = scopes.at(scopes.size() - 2);
				scopes.back()->set_parent(parent);
			}
		}

		void pop()
		{
			scopes.pop_back();
		}

		std::optional<std::reference_wrapper<types::type>> typeof(const extended_ast::identifier& name)
		{
			if (!name.scope_distance.has_value()) return std::nullopt;
			

			auto res = scopes.back()->typeof(name, name.scope_distance.value());
			if (res.has_value()) return res.value();

			if (auto loc = modules.find(name.segments.front()); loc != modules.end())
				return loc->second.typeof(name.without_first_segment());

			return std::nullopt;
		}

		void set_type(const extended_ast::identifier& name, types::unique_type type, std::size_t scope_depth = 0)
		{
			this->scopes.at(this->scopes.size() - 1 - scope_depth)->set_type(name, std::move(type));
		}

		std::optional<std::reference_wrapper<types::type>> resolve_type(const extended_ast::identifier& name)
		{
			auto within_module = this->scopes.back()->resolve_type(name);
			if (within_module.has_value()) return within_module.value();

			if (auto loc = modules.find(name.segments.front()); loc != modules.end())
				return loc->second.resolve_type(name.without_first_segment());

			return std::nullopt;
		}

		void define_type(const extended_ast::identifier& name, types::unique_type type)
		{
			this->scopes.back()->define_type(name, std::move(type));
		}

		void add_global_module(const type_environment& other)
		{
			scopes.front()->merge(*other.scopes.front());
		}

		void add_module(const std::string& name, const type_environment& other)
		{
			modules.insert({ name, other });
		}

		std::string to_string(bool include_modules = false)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};

			std::string r = "type_environment (";

			if (include_modules)
			{
				r.append(indent("\n" + std::string("modules (")));
				for (auto& string_namespace_pair : modules)
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

	private:
		std::vector<std::shared_ptr<detail::type_scope>> scopes;
		std::unordered_map<std::string, type_environment> modules;
	};
}
/*
namespace fe
{
	class type_environment
	{
	public:
		type_environment() {}
		type_environment(std::optional<std::string> name) : name(name) {}
		type_environment(std::unordered_map<std::string, types::unique_type> type_mapping)
			: types(std::move(type_mapping)) {}
		type_environment(const type_environment& other) : name(other.name)
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

		type_environment& operator=(const type_environment& other)
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

		type_environment& operator=(type_environment&& other)
		{
			this->types = std::move(other.types);
			this->namespaces = std::move(other.namespaces);
			this->name = std::move(other.name);
			return *this;
		}

		void add_module(type_environment&& other)
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

		std::variant<std::reference_wrapper<types::type>, type_env_error>
			typeof(const extended_ast::identifier& id) const
		{
			if (id.segments.size() == 1 && id.segments.at(0) == "_")
				return type_env_error{ "Cannot use identifier _" };

			if (id.segments.size() == 1)
			{
				if (types.find(id.segments.at(0)) == types.end())
					return type_env_error{ "Identifier " + id.to_string() + " is unbounded" };

				return *types.at(id.segments.at(0));
			}

			if (namespaces.find(id.segments.at(0)) != namespaces.end())
			{
				return namespaces
					.find(id.segments.at(0))->second
					.typeof(id.without_first_segment());
			}

			std::reference_wrapper<types::type> type = *types.at(id.segments.at(0));

			for (auto i = 1; i < id.segments.size(); i++)
			{
				const auto product_type = dynamic_cast<const types::product_type*>(&type.get());
				auto& type_location = product_type->product.at(id.offsets.at(i - 1));

				type = *type_location;
			}

			return type;
		}

		std::string to_string(const bool include_modules = false)
		{
			auto indent = [](std::string& text) {
				return std::regex_replace(text, std::regex("\\n"), "\n\t");
			};

			auto r = name.has_value() ?
				"type_environment: " + name.value() + " (" :
				"type_environment (";

			uint32_t counter = 0;
			for (decltype(auto) pair : types)
			{
				auto t = "\n\t" + pair.first + ": ";
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
		std::unordered_multimap<std::string, type_environment> namespaces;
	};
}
*/
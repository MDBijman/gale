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

		std::optional<std::reference_wrapper<types::type>> typeof(const extended_ast::identifier& name,
			int scope_depth)
		{
			if (scope_depth > 0)
			{
				return parent.has_value()
					? parent.value()->typeof(name, scope_depth - 1)
					: std::nullopt;
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

		void add_module(std::vector<std::string> name, type_environment other)
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
					type_environment new_te;

					new_te.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));

					modules.insert({ std::move(name[0]), std::move(new_te) });
				}
			}
		}

		void add_module(std::string name, type_environment other)
		{
			add_module({ std::move(name) }, std::move(other));
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

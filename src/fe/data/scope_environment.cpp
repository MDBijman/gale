#include "fe/data/scope_environment.h"
#include <optional>
#include <tuple>
#include <memory>

namespace fe
{
	void nested_type::insert(std::string field, nested_type t)
	{
		names.push_back(std::pair<std::string, nested_type>(field, t));
	}

	void nested_type::insert(std::string field)
	{
		names.push_back(field);
	}

	std::optional<std::vector<int>> nested_type::resolve(const extended_ast::identifier& name) const
	{
		if (name.segments.size() == 0) return std::nullopt;

		for (auto it = names.begin(); it != names.end(); it++)
		{
			auto& elem = *it;

			if (std::holds_alternative<std::string>(elem))
			{
				if (name.segments.size() > 1) continue;

				auto field = std::get<std::string>(elem);
				if (field == name.segments.at(0))
					return std::vector<int>({ static_cast<int>(std::distance(names.begin(), it)) });
			}
			else
			{
				auto nested = std::get<std::pair<std::string, nested_type>>(elem);
				if (nested.first != name.segments.at(0))
					continue;

				if (name.segments.size() != 1)
				{
					auto nested_resolved = nested.second.resolve(name.without_first_segment());
					if (!nested_resolved.has_value()) return std::nullopt;
					nested_resolved.value().insert(nested_resolved.value().begin(),
						static_cast<int>(std::distance(names.begin(), it)));
					return nested_resolved.value();
				}
				else
				{
					return std::vector<int>({ static_cast<int>(std::distance(names.begin(), it)) });
				}
			}
		}

		return std::nullopt;
	}
}

namespace fe::detail
{
	scope::scope() {}
	scope::scope(const scope& other) : identifiers(other.identifiers), parent(other.parent)
	{
		for (decltype(auto) pair : other.nested_types)
			this->nested_types.insert(pair);
	}

	void scope::merge(scope other)
	{
		for (auto&& id : other.identifiers)
			this->identifiers.insert(std::move(id));
		for (auto&& pair : other.nested_types)
			this->nested_types.insert(std::move(pair));
	}

	void scope::merge(std::vector<std::string> name, scope other)
	{
		for (auto id : other.identifiers)
		{
			this->identifiers.insert(std::move(id));
		}
		for (auto pair : other.nested_types)
		{
			this->nested_types.insert(std::move(pair));
		}
	}

	void scope::merge(std::string name, scope other)
	{
		this->merge(std::vector<std::string>{ std::move(name) }, std::move(other));
	}

	void scope::set_parent(scope* other)
	{
		this->parent = other;
	}

	/*
	* Returns the type name of the given reference.
	*/
	std::optional<std::pair<std::size_t, extended_ast::identifier>> scope::resolve_reference(
		const extended_ast::identifier& id) const
	{
		if (id.segments.size() == 0) return std::nullopt;

		const auto loc = identifiers.find(id.segments.at(0));
		if (loc == identifiers.end())
		{
			if (!parent.has_value())
				return std::nullopt;

			auto res = parent.value()->resolve_reference(id);

			if (!res.has_value())
				return std::nullopt;

			res.value().first++;

			return res;
		}

		// The variable has been declared but not defined 
		if (loc->second.second == false)
			throw resolution_error{ "Variable referenced in its own definition" };

		return std::make_pair(std::size_t(0), loc->second.first);
	}

	std::optional<std::pair<std::size_t, nested_type>> scope::resolve_type(const extended_ast::identifier& id) const
	{
		auto type_location = nested_types.find(id.segments[0]);

		if (type_location != nested_types.end())
			return std::make_pair(0, type_location->second);

		if (!parent.has_value())
			return std::nullopt;

		auto parent_resolve = parent.value()->resolve_type(id);

		if (!parent_resolve.has_value())
			return std::nullopt;

		parent_resolve.value().first++;

		return parent_resolve;
	}

	/*
	* Declares the variable with the given name within this scope. The variable will not yet be resolvable.
	*/
	void scope::declare_reference(std::string id, extended_ast::identifier type_name)
	{
		if (identifiers.find(id) != identifiers.end())
			throw resolution_error{ "Cannot shadow variable in same scope" };

		identifiers.insert({ std::move(id), { std::move(type_name), false } });
	}

	/*
	* Defines the given name within this scope. After this, the variable will be resolvable.
	*/
	void scope::define_reference(const std::string& id)
	{
		identifiers.at(id).second = true;
	}

	/*
	* TODO
	*/
	void scope::define_type(std::string id, nested_type t)
	{
		if (nested_types.find(id) != nested_types.end())
			throw resolution_error{ "Cannot shadow type in same scope" };

		nested_types.insert({ std::move(id), std::move(t) });
	}
}

namespace fe
{
	scope_environment::scope_environment()
	{
		push();
	}

	scope_environment::scope_environment(const scope_environment& other) : modules(other.modules)
	{
		for (auto& pair : other.scopes)
		{
			this->scopes.push_back(std::make_unique<detail::scope>(*pair));
		}
	}

	void scope_environment::push()
	{
		scopes.push_back(std::make_unique<detail::scope>());

		// There is a parent
		if (scopes.size() > 1) {
			auto& parent = scopes.at(scopes.size() - 2);
			scopes.back()->set_parent(parent.get());
		}
	}

	void scope_environment::pop()
	{
		scopes.pop_back();
	}

	std::optional<std::pair<std::size_t, std::vector<int>>> scope_environment::resolve_reference(
		const extended_ast::identifier& name) const
	{
		// Check scopes
		std::optional<std::pair<std::size_t, extended_ast::identifier>> resolved = scopes.back()->resolve_reference(name);
		if (resolved.has_value())
		{
			// Atom type
			if (name.segments.size() == 1)
				return std::make_pair(resolved.value().first, std::vector<int>{});

			// Nested type
			std::optional<std::pair<std::size_t, nested_type>> resolved_type = resolve_type(resolved.value().second);
			if (!resolved_type.has_value())
				return std::nullopt;

			std::optional<std::vector<int>> offsets = resolved_type.value().second.resolve(name.without_first_segment());
			assert(offsets.has_value());

			return std::make_pair(resolved.value().first, offsets.value());
		}

		// Check modules
		if (auto loc = modules.find(name.segments.front()); loc != modules.end())
		{
			auto res = loc->second.resolve_reference(name.without_first_segment());

			if (!res.has_value())
				return std::nullopt;

			res.value().first = scopes.size() - 1;

			return res;
		}

		return std::nullopt;
	}

	std::optional<std::pair<std::size_t, nested_type>> scope_environment::resolve_type(
		const extended_ast::identifier& name) const
	{
		auto& resolved = scopes.back()->resolve_type(name);
		if (resolved.has_value())
		{
			return resolved.value();
		}

		if (auto loc = modules.find(name.segments.front()); loc != modules.end())
		{
			auto new_id = name.without_first_segment();
			new_id.scope_distance = 0;
			resolved = loc->second.resolve_type(std::move(new_id));

			if (!resolved.has_value())
				return std::nullopt;

			resolved.value().first = scopes.size() - 1;

			return resolved.value();
		}

		return std::nullopt;
	}

	std::optional<nested_type> scope_environment::get_type(const extended_ast::identifier& name) const
	{

		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
		{
			auto offsets = (*it)->resolve_type(name);
			if (offsets.has_value()) return offsets.value().second;
		}

		if (auto loc = modules.find(name.segments.front()); loc != modules.end())
			return loc->second.get_type(name.without_first_segment());

		return std::nullopt;
	}

	void scope_environment::define_type(const extended_ast::identifier& id, const extended_ast::unique_node& content)
	{
		// This function builds a nested_type object from an AST node
		using field_type = std::variant<nested_type, std::string, std::pair<std::string, nested_type>>;
		std::function<field_type(extended_ast::node*)> build_fields = [&](extended_ast::node* type) -> field_type
		{
			if (auto tuple_declaration = dynamic_cast<extended_ast::tuple_declaration*>(type))
			{
				nested_type t;
				for (auto i = 0; i < tuple_declaration->elements.size(); i++)
				{
					decltype(auto) tuple_declaration_element = tuple_declaration->elements.at(i);
					auto fields = build_fields(tuple_declaration_element.get());

					if (std::holds_alternative<std::string>(fields))
						t.insert(std::get<std::string>(fields));
					else if (std::holds_alternative<nested_type>(fields))
					{
						auto nested_fields = std::get<nested_type>(fields);
						for (auto& field : nested_fields.names)
						{
							if (std::holds_alternative<std::string>(field))
							{
								t.insert(std::get<std::string>(field));
							}
							else if (std::holds_alternative<std::pair<std::string, nested_type>>(field))
							{
								auto[name, content] = std::get<std::pair<std::string, nested_type>>(field);
								t.insert(name, content);
							}
						}
					}
					else if (std::holds_alternative<std::pair<std::string, nested_type>>(fields))
					{
						auto nested_fields = std::get<std::pair<std::string, nested_type>>(fields);
						t.insert(nested_fields.first, nested_fields.second);
					}
				}
				return t;
			}
			else if (auto atom_declaration = dynamic_cast<extended_ast::atom_declaration*>(type))
			{
				auto scope_depth = build_fields(atom_declaration->type_expression.get());

				return std::pair<std::string, nested_type>(atom_declaration->name.segments.at(0),
					std::get<nested_type>(scope_depth));
			}
			else if (auto atom = dynamic_cast<extended_ast::type_atom*>(type))
			{
				auto type_name = dynamic_cast<extended_ast::identifier*>(atom->type.get());
				auto type = this->get_type(*type_name);
				assert(type.has_value());
				return type.value();
			}
			else if (auto ref = dynamic_cast<extended_ast::reference_type*>(type))
			{
				return build_fields(ref->child.get());
			}
			else if (auto arr = dynamic_cast<extended_ast::array_type*>(type))
			{
				return build_fields(arr->child.get());
			}
		};

		auto nt = build_fields(content.get());
		if (std::holds_alternative<std::string>(nt))
		{
			nested_type t;
			t.insert(std::get<std::string>(nt));
			define_type(id, t);
		}
		else if (std::holds_alternative<nested_type>(nt))
		{
			define_type(id, std::get<nested_type>(nt));
		}
		else if (std::holds_alternative<std::pair<std::string, nested_type>>(nt))
		{
			auto res = std::get<std::pair<std::string, nested_type>>(nt);
			nested_type t;
			t.insert(res.first, res.second);
			define_type(id, std::get<nested_type>(res));
		}
	}

	void scope_environment::define_type(const extended_ast::identifier& id, const nested_type& content)
	{
		scopes.back()->define_type(id.segments[0], content);
	}

	void scope_environment::declare(extended_ast::identifier id, extended_ast::identifier type_name)
	{
		scopes.back()->declare_reference(std::move(id.segments[0]), std::move(type_name));
	}

	void scope_environment::define(const extended_ast::identifier_tuple& id)
	{
		for (auto& child : id.content)
			std::visit([this](auto& c) { this->define(c); }, child);
	}

	void scope_environment::define(const extended_ast::identifier& id)
	{
		scopes.back()->define_reference(id.segments[0]);
	}

	void scope_environment::add_global_module(scope_environment mod)
	{
		scopes.front()->merge(*mod.scopes.front());
	}

	void scope_environment::add_module(std::vector<std::string> name, scope_environment other)
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
				scope_environment new_se;

				new_se.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));

				modules.insert({ std::move(name[0]), std::move(new_se) });
			}
		}
	}

	void scope_environment::add_module(std::string name, scope_environment other)
	{
		add_module({ std::move(name) }, std::move(other));
	}

	std::size_t scope_environment::depth()
	{
		return this->scopes.size();
	}
}
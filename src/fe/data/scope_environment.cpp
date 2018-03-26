#include "fe/data/scope_environment.h"
#include "fe/data/extended_ast.h"
#include <optional>
#include <tuple>
#include <memory>

namespace fe::resolution
{
	/*
	* nested_type implementation
	*/

	nested_type::nested_type() {}

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

namespace fe::resolution::detail
{
	/*
	* node implementation
	*/

	node::node() {}
	
	node::node(const node& o) : parent(std::nullopt)
	{
		for (decltype(auto) child : o.children)
		{
			this->children.push_back(std::unique_ptr<node>(child->copy()));
			this->children.rbegin()->get()->set_parent(this);
		}
	}

	node::node(node&& o) : parent(std::nullopt), children(std::move(o.children))
	{
		for (decltype(auto) child : o.children)
		{
			child->set_parent(this);
		}
	}

	node& node::operator=(const node& o)
	{
		this->parent = std::nullopt;
		for (decltype(auto) child : o.children)
		{
			this->children.push_back(std::unique_ptr<node>(child->copy()));
			this->children.rbegin()->get()->set_parent(this);
		}
		return *this;
	}

	node& node::operator=(node&& o)
	{
		this->parent = std::nullopt;
		for (auto&& child : o.children)
		{
			this->children.push_back(std::move(child));
			this->children.rbegin()->get()->set_parent(this);
		}
		return *this;
	}

	void node::set_parent(node* o) { this->parent = o; }

	std::optional<node*> node::get_parent() { return this->parent; }

	void node::add_child(node* n)
	{
		this->children.push_back(std::unique_ptr<node>(n));
	}

	node& node::operator[](std::size_t i)
	{
		return *children[i].get();
	}

	/*
	* scoped_node implementation
	*/

	node* scoped_node::copy()
	{
		return new scoped_node(*this);
	}

	scoped_node::scoped_node(const scoped_node& other) : node(other), identifiers(other.identifiers),
		nested_types(other.nested_types) {}

	scoped_node& scoped_node::operator=(const scoped_node& o)
	{
		node::operator=(o);
		this->identifiers = o.identifiers;
		this->nested_types = o.nested_types;
		return *this;
	}

	void scoped_node::merge(scoped_node& other)
	{
		for (auto&& id : other.identifiers)
			this->identifiers.insert(std::move(id));
		for (auto&& pair : other.nested_types)
			this->nested_types.insert(std::move(pair));
	}

	void scoped_node::merge(std::vector<std::string> name, scoped_node& other)
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

	void scoped_node::merge(std::string name, scoped_node& other)
	{
		this->merge(std::vector<std::string>{ std::move(name) }, std::move(other));
	}

	std::optional<var_lookup_res> scoped_node::resolve_var_id(const extended_ast::identifier& id) const
	{
		if (id.segments.size() == 0) return std::nullopt;

		const auto loc = identifiers.find(id.segments.at(0));
		if (loc == identifiers.end())
		{
			if (!parent.has_value())
				return std::nullopt;

			auto res = parent.value()->resolve_var_id(id);

			if (!res.has_value())
				return std::nullopt;

			res.value().scope_distance++;

			return res;
		}

		// The variable has been declared but not defined 
		if (loc->second.second == false)
			throw resolution_error{ "Variable referenced in its own definition" };

		return var_lookup_res{ 0, loc->second.first };
	}

	void scoped_node::define_type(std::string id, const extended_ast::unique_node& t)
	{
		if (nested_types.find(id) != nested_types.end())
			throw resolution_error{ "Cannot shadow type in same scope" };

		// This function builds a nested_type object from an AST node
		using named_field = std::pair<std::string, nested_type>;

		// #todo #fixme ugly
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
				auto type_res = this->resolve_type(*type_name);
				assert(type_res.has_value());
				return type_res.value().type_structure;
			}
			else if (auto ref = dynamic_cast<extended_ast::reference_type*>(type))
			{
				return build_fields(ref->child.get());
			}
			else if (auto arr = dynamic_cast<extended_ast::array_type*>(type))
			{
				return build_fields(arr->child.get());
			}

			assert(!"Invalid node inside type declaration");
		};

		auto nt = build_fields(t.get());
		if (std::holds_alternative<std::string>(nt))
		{
			nested_type wrapper_nt;
			wrapper_nt.insert(std::get<std::string>(nt));
			nested_types.insert({ std::move(id), std::move(wrapper_nt) });
		}
		else if (std::holds_alternative<nested_type>(nt))
		{
			nested_types.insert({ std::move(id), std::get<nested_type>(nt) });
		}
		else if (std::holds_alternative<std::pair<std::string, nested_type>>(nt))
		{
			auto res = std::get<std::pair<std::string, nested_type>>(nt);
			nested_type t;
			t.insert(res.first, res.second);
			nested_types.insert({ std::move(id), std::move(t) });
		}
	}

	void scoped_node::define_type(std::string id, const nested_type & t)
	{
		nested_types.insert({ std::move(id), t });
	}

	std::optional<type_lookup_res> scoped_node::resolve_type(const extended_ast::identifier& id) const
	{
		auto type_location = nested_types.find(id.segments[0]);

		if (type_location != nested_types.end())
			return type_lookup_res{ 0, type_location->second };

		if (!parent.has_value())
			return std::nullopt;

		auto parent_resolve = parent.value()->resolve_type(id);

		if (!parent_resolve.has_value())
			return std::nullopt;

		parent_resolve.value().scope_distance++;

		return parent_resolve;
	}

	void scoped_node::declare_var_id(std::string id, const extended_ast::identifier& type_name)
	{
		if (identifiers.find(id) != identifiers.end())
			throw resolution_error{ "Cannot shadow variable in same scope" };

		identifiers.insert({ id, { std::move(type_name), false } });
	}

	void scoped_node::define_var_id(const std::string& id)
	{
		identifiers.at(id).second = true;
	}

	///*
	//* root_node implementation
	//*
	//* Cannot define/declare new types/variables. Only holds modules.
	//*/

	//root_node::root_node(const root_node& other) : modules(other.modules)
	//{}

	//root_node& root_node::operator=(const root_node& other)
	//{
	//	this->modules = other.modules;
	//	return *this;
	//}

	//node* root_node::copy()
	//{
	//	return new root_node(*this);
	//}

	//void root_node::declare_var_id(std::string id, const extended_ast::identifier& type_name)
	//{
	//	assert(!"Module node reached, fatal error");
	//}

	//void root_node::define_var_id(const std::string& id)
	//{
	//	assert(!"Module node reached, fatal error");
	//}

	//std::optional<var_lookup_res> root_node::resolve_var_id(const extended_ast::identifier& id) const
	//{
	//	if (auto loc = modules.find(name.segments.front()); loc != modules.end())
	//	{
	//		auto res = loc->second.resolve_reference(name.without_first_segment());

	//		if (!res.has_value())
	//			return std::nullopt;

	//		res.value().scope_distance = scopes.size() - 1;

	//		return res;
	//	}

	//	return std::nullopt;
	//}

	//void root_node::define_type(std::string id, const extended_ast::unique_node& t)
	//{
	//	assert(!"Module node reached, fatal error");
	//}

	//void root_node::define_type(std::string id, const nested_type& t)
	//{
	//	assert(!"Module node reached, fatal error");
	//}

	//std::optional<type_lookup_res> root_node::resolve_type(const extended_ast::identifier& id) const
	//{
	//	if (auto loc = modules.find(name.segments.front()); loc != modules.end())
	//	{
	//		auto new_id = name.without_first_segment();
	//		new_id.scope_distance = 0;
	//		resolved = loc->second.resolve_type(std::move(new_id));

	//		if (!resolved.has_value())
	//			return std::nullopt;

	//		resolved.value().scope_distance = scopes.size() - 1;

	//		return resolved.value();
	//	}

	//	return std::nullopt;
	//}

	/*
	* unscoped_node implementation
	*
	* Simply delegates all calls to the parent node.
	*/

	node* unscoped_node::copy()
	{
		return new unscoped_node(*this);
	}

	void unscoped_node::declare_var_id(std::string id, const extended_ast::identifier& type_name)
	{
		assert(this->parent.has_value());
		this->parent.value()->declare_var_id(id, type_name);
	}

	void unscoped_node::define_var_id(const std::string& id)
	{
		assert(this->parent.has_value());
		this->parent.value()->define_var_id(id);
	}

	std::optional<var_lookup_res> unscoped_node::resolve_var_id(const extended_ast::identifier& id) const
	{
		assert(this->parent.has_value());
		return this->parent.value()->resolve_var_id(id);
	}

	void unscoped_node::define_type(std::string id, const extended_ast::unique_node& t)
	{
		assert(this->parent.has_value());
		this->parent.value()->define_type(id, t);
	}

	std::optional<type_lookup_res> unscoped_node::resolve_type(const extended_ast::identifier& id) const
	{
		assert(this->parent.has_value());
		return this->parent.value()->resolve_type(id);
	}

	/*
	* Implementation of iterator
	*/

	//std::optional<var_resolve_res> node_iterator::resolve_reference(const extended_ast::identifier& name) const
	//{
	//	// Check scopes
	//	std::optional<detail::var_lookup_res> resolved = scopes.back()->resolve_var_id(name);
	//	if (resolved.has_value())
	//	{
	//		// Atom type
	//		if (name.segments.size() == 1)
	//			return var_resolve_res{ resolved.value().scope_distance, std::vector<int>{} };

	//		// Nested type
	//		std::optional<type_resolve_res> resolved_type = resolve_type(resolved.value().type_name);
	//		if (!resolved_type.has_value())
	//			return std::nullopt;

	//		std::optional<std::vector<int>> offsets = resolved_type.value().type_structure
	//			.resolve(name.without_first_segment());
	//		assert(offsets.has_value());

	//		return var_resolve_res{ resolved.value().scope_distance, offsets.value() };
	//	}

		//// Check modules
		//if (auto loc = modules.find(name.segments.front()); loc != modules.end())
		//{
		//	auto res = loc->second.resolve_reference(name.without_first_segment());

		//	if (!res.has_value())
		//		return std::nullopt;

		//	res.value().scope_distance = scopes.size() - 1;

		//	return res;
		//}

	//	return std::nullopt;
	//}

	//std::optional<type_resolve_res> node_iterator::resolve_type(const extended_ast::identifier& name) const
	//{
	//	auto& resolved = scopes.back()->resolve_type(name);
	//	if (resolved.has_value())
	//		return resolved.value();

	//	//if (auto loc = modules.find(name.segments.front()); loc != modules.end())
	//	//{
	//	//	auto new_id = name.without_first_segment();
	//	//	new_id.scope_distance = 0;
	//	//	resolved = loc->second.resolve_type(std::move(new_id));

	//	//	if (!resolved.has_value())
	//	//		return std::nullopt;

	//	//	resolved.value().scope_distance = scopes.size() - 1;

	//	//	return resolved.value();
	//	//}

	//	return std::nullopt;
	//}

	//void node_iterator::define_type(const extended_ast::identifier& id, const extended_ast::unique_node& content)
	//{
	//	// This function builds a nested_type object from an AST node
	//	using named_field = std::pair<std::string, nested_type>;

	//	// #todo #fixme ugly
	//	using field_type = std::variant<nested_type, std::string, std::pair<std::string, nested_type>>;
	//	std::function<field_type(extended_ast::node*)> build_fields = [&](extended_ast::node* type) -> field_type
	//	{
	//		if (auto tuple_declaration = dynamic_cast<extended_ast::tuple_declaration*>(type))
	//		{
	//			nested_type t;
	//			for (auto i = 0; i < tuple_declaration->elements.size(); i++)
	//			{
	//				decltype(auto) tuple_declaration_element = tuple_declaration->elements.at(i);
	//				auto fields = build_fields(tuple_declaration_element.get());

	//				if (std::holds_alternative<std::string>(fields))
	//					t.insert(std::get<std::string>(fields));
	//				else if (std::holds_alternative<nested_type>(fields))
	//				{
	//					auto nested_fields = std::get<nested_type>(fields);
	//					for (auto& field : nested_fields.names)
	//					{
	//						if (std::holds_alternative<std::string>(field))
	//						{
	//							t.insert(std::get<std::string>(field));
	//						}
	//						else if (std::holds_alternative<std::pair<std::string, nested_type>>(field))
	//						{
	//							auto[name, content] = std::get<std::pair<std::string, nested_type>>(field);
	//							t.insert(name, content);
	//						}
	//					}
	//				}
	//				else if (std::holds_alternative<std::pair<std::string, nested_type>>(fields))
	//				{
	//					auto nested_fields = std::get<std::pair<std::string, nested_type>>(fields);
	//					t.insert(nested_fields.first, nested_fields.second);
	//				}
	//			}
	//			return t;
	//		}
	//		else if (auto atom_declaration = dynamic_cast<extended_ast::atom_declaration*>(type))
	//		{
	//			auto scope_depth = build_fields(atom_declaration->type_expression.get());

	//			return std::pair<std::string, nested_type>(atom_declaration->name.segments.at(0),
	//				std::get<nested_type>(scope_depth));
	//		}
	//		else if (auto atom = dynamic_cast<extended_ast::type_atom*>(type))
	//		{
	//			auto type_name = dynamic_cast<extended_ast::identifier*>(atom->type.get());
	//			auto type_res = this->resolve_type(*type_name);
	//			assert(type_res.has_value());
	//			return type_res.value().type_structure;
	//		}
	//		else if (auto ref = dynamic_cast<extended_ast::reference_type*>(type))
	//		{
	//			return build_fields(ref->child.get());
	//		}
	//		else if (auto arr = dynamic_cast<extended_ast::array_type*>(type))
	//		{
	//			return build_fields(arr->child.get());
	//		}

	//		assert(!"Invalid node inside type declaration");
	//	};

	//	auto nt = build_fields(content.get());
	//	if (std::holds_alternative<std::string>(nt))
	//	{
	//		nested_type t;
	//		t.insert(std::get<std::string>(nt));
	//		define_type(id, t);
	//	}
	//	else if (std::holds_alternative<nested_type>(nt))
	//	{
	//		define_type(id, std::get<nested_type>(nt));
	//	}
	//	else if (std::holds_alternative<std::pair<std::string, nested_type>>(nt))
	//	{
	//		auto res = std::get<std::pair<std::string, nested_type>>(nt);
	//		nested_type t;
	//		t.insert(res.first, res.second);
	//		define_type(id, std::get<nested_type>(res));
	//	}
	//}

	//void node_iterator::define_type(const extended_ast::identifier& id, const nested_type& content)
	//{
	//	current->define_type(id.segments[0], content);
	//}

	//void node_iterator::declare(extended_ast::identifier id, extended_ast::identifier type_name)
	//{
	//	current->declare_var_id(std::move(id.segments[0]), std::move(type_name));
	//}

	//void node_iterator::define(const extended_ast::identifier_tuple& id)
	//{
	//	for (auto& child : id.content)
	//		std::visit([this](auto& c) { this->define(c); }, child);
	//}

	//void node_iterator::define(const extended_ast::identifier& id)
	//{
	//	current->define_var_id(id.segments[0]);
	//}
}

/*
* Implementation of scope environment
*/
namespace fe::resolution
{
	/*
	* Implementation of scope environment
	*/

	scope_environment::scope_environment() {}

	scope_environment::scope_environment(std::unique_ptr<scope_tree_node> r) : root(std::move(r)) {}

	scope_environment::scope_environment(scope_environment&& other) : root(std::move(other.root))
	{}

	scope_environment::scope_environment(const scope_environment& other) :
		root(other.root->copy())
	{}

	scope_tree_node& scope_environment::get_root()
	{
		return *this->root;
	}

	//void scope_environment::add_module(std::vector<std::string> name, scope_environment other)
	//{
	//	this->root.add_module(name, other);
	//	//if (name.size() == 0)
	//	//{
	//	//	add_global_module(std::move(other));
	//	//}
	//	//else
	//	//{
	//	//	if (auto loc = modules.find(name[0]); loc != modules.end())
	//	//	{
	//	//		loc->second.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));
	//	//	}
	//	//	else
	//	//	{
	//	//		scope_environment new_se{ scoped_node() };

	//	//		new_se.add_module(std::vector<std::string>(name.begin() + 1, name.end()), std::move(other));

	//	//		modules.insert({ std::move(name[0]), std::move(new_se) });
	//	//	}
	//	//}
	//}
}
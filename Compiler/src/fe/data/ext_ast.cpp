#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	ast::ast() {}
	ast::ast(ast_allocation_hints h)
	{
		nodes.reserve(h.nodes);
		name_scopes.reserve(h.name_scopes);
		type_scopes.reserve(h.type_scopes);

		constants.identifiers.reserve(h.identifiers);
		constants.booleans.reserve(h.booleans);
		constants.strings.reserve(h.strings);
		constants.numbers.reserve(h.numbers);
	}

	void ast::set_root_id(node_id id)
	{
		root = id;
	}

	// Root node
	node_id ast::root_id()
	{
		return root;
	}

	// Nodes
	node_children& ast::get_children(children_id id)
	{
		assert(id != no_children);
		return children.get_at(id);
	}

	node_children& ast::children_of(node_id id)
	{
		assert(id != no_node);
		return children_of(get_node(id));
	}

	node_children& ast::children_of(node& n)
	{
		return get_children(n.children_id);
	}

	node_id ast::create_node(node_type t)
	{
		auto new_node = static_cast<node_id>(nodes.create());
		auto& node = get_node(new_node);
		node.id = new_node;
		node.kind = t;
		node.data_index = create_node_data(t);
		node.children_id = is_terminal_node(t) ? no_children : children.create();
		return new_node;
	}

	node& ast::get_node(node_id id)
	{
		assert(id != no_node);
		return nodes.get_at(id);
	}

	std::optional<identifier> ast::get_module_name()
	{
		auto module_dec_id = find_node(node_type::MODULE_DECLARATION);
		if (!module_dec_id.has_value()) return std::nullopt;
		auto& id_node = get_node(children_of(module_dec_id.value())[0]);
		return get_data<identifier>(id_node.data_index);
	}

	std::optional<std::vector<identifier>> ast::get_imports()
	{
		auto import_dec_id = find_node(node_type::IMPORT_DECLARATION);
		if (!import_dec_id.has_value()) return std::nullopt;

		std::vector<identifier> imports;
		auto& children = children_of(import_dec_id.value());
		for (auto child : children)
		{
			auto& id_node = get_node(child);
			imports.push_back(get_data<identifier>(id_node.data_index));
		}
		return imports;
	}

	// Scopes
	scope_index ast::create_name_scope()
	{
		return static_cast<scope_index>(name_scopes.create());
	}

	scope_index ast::create_name_scope(scope_index parent)
	{
		assert(parent != no_scope);
		auto new_scope = name_scopes.create();
		name_scopes.get_at(new_scope).set_parent(parent);
		return static_cast<scope_index>(new_scope);
	}

	name_scope& ast::get_name_scope(scope_index id)
	{
		assert(id != no_scope);
		return name_scopes.get_at(id);
	}

	name_scope::get_scope_cb ast::name_scope_cb()
	{
		return [&](scope_index i) { return &name_scopes.get_at(i); };
	}

	scope_index ast::create_type_scope()
	{
		return static_cast<scope_index>(type_scopes.create());
	}

	scope_index ast::create_type_scope(scope_index parent)
	{
		assert(parent != no_scope);
		auto new_scope = type_scopes.create();
		type_scopes.get_at(new_scope).set_parent(parent);
		return static_cast<uint32_t>(new_scope);
	}

	type_scope& ast::get_type_scope(scope_index id)
	{
		assert(id != no_scope);
		return type_scopes.get_at(id);
	}

	type_scope::get_scope_cb ast::type_scope_cb()
	{
		return [&](scope_index i) { return &type_scopes.get_at(i); };
	}

	// Node data 
	constants_store& ast::get_constants()
	{
		return this->constants;
	}

	data_index_t ast::create_node_data(node_type t)
	{
		switch (t)
		{
		case node_type::IDENTIFIER: return static_cast<data_index_t>(identifiers.create());
		case node_type::NUMBER:     return static_cast<data_index_t>(constants.create<number>());
		case node_type::STRING:     return static_cast<data_index_t>(constants.create<string>());
		case node_type::BOOLEAN:    return static_cast<data_index_t>(constants.create<boolean>());
		default:
			return no_data;
		}
	}

	std::optional<node_id> ast::find_node(node_type t)
	{
		for (node_id i = 0; i < nodes.get_data().size(); i++)
		{
			if (!nodes.is_occupied(i))
				continue;
			if (nodes.get_at(i).kind == t)
				return i;
		}
		return std::nullopt;
	}

	namespace detail
	{
		template<> identifier& get_data<identifier>(ast& a, data_index_t i) { assert(i != no_data); return a.identifiers.get_at(i); }
		template<> boolean&    get_data<boolean>(ast& a, data_index_t i) { assert(i != no_data); return a.constants.get<boolean>(i); }
		template<> string&     get_data<string>(ast& a, data_index_t i) { assert(i != no_data); return a.constants.get<string>(i); }
		template<> number&     get_data<number>(ast& a, data_index_t i) { assert(i != no_data); return a.constants.get<number>(i); }
	}
}
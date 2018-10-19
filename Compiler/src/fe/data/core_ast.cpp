#include "fe/data/core_ast.h"

namespace fe::core_ast
{
	ast::ast(node_type t)
	{
		root = static_cast<node_id>(nodes.create());
		nodes.get_at(root) = node(t);
		nodes.get_at(root).id = root;
		nodes.get_at(root).data_index = create_node_data(t);
	}

	node_id ast::root_id()
	{
		return root;
	}

	// Nodes
	node_id ast::create_node(node_type t)
	{
		auto new_node = static_cast<node_id>(nodes.create());
		get_node(new_node).id = new_node;
		get_node(new_node).kind = t;
		get_node(new_node).data_index = create_node_data(t);
		return new_node;
	}

	node_id ast::create_node(node_type t, node_id parent)
	{
		auto new_node = static_cast<node_id>(nodes.create());
		get_node(new_node).id = new_node;
		get_node(new_node).kind = t;
		get_node(new_node).data_index = create_node_data(t);
		get_node(new_node).parent_id = parent;
		get_node(parent).children.push_back(new_node);
		return new_node;
	}

	node& ast::parent_of(node_id id)
	{
		return nodes.get_at(*nodes.get_at(id).parent_id);
	}

	std::vector<node_id>& ast::children_of(node_id id)
	{
		return nodes.get_at(id).children;
	}

	void ast::link_child_parent(node_id child, node_id parent)
	{
		children_of(parent).push_back(child);
		get_node(child).parent_id = parent;
	}

	node& ast::get_node(node_id id)
	{
		return nodes.get_at(id);
	}

	std::optional<data_index> ast::create_node_data(node_type t)
	{
		switch (t)
		{
		case node_type::NUMBER: return constants.create<number>();
		case node_type::STRING: return constants.create<string>();
		case node_type::BOOLEAN: return constants.create<boolean>();
		case node_type::FUNCTION: return function_data_store.create();
		case node_type::FUNCTION_CALL: return function_call_data_store.create();
		case node_type::JMP:
		case node_type::JNZ:
		case node_type::JZ:
		case node_type::LABEL:
			return label_store.create();
		case node_type::STACK_DEALLOC:
		case node_type::STACK_ALLOC:
		case node_type::MOVE: 
		case node_type::POP:
		case node_type::LOCAL_ADDRESS:
		case node_type::REGISTER:
			return size_store.create();
		case node_type::RET:
			return return_data_store.create();
		default: return std::nullopt;
		}
	}
}
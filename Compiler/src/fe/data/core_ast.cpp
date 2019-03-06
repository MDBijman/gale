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

	std::optional<data_index_t> ast::create_node_data(node_type t)
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
		case node_type::RELATIVE_OFFSET:
			return relative_offset_store.create();
		case node_type::STACK_LABEL:
			return stack_label_store.create();
		case node_type::STACK_DEALLOC:
		case node_type::STACK_ALLOC:
		case node_type::PUSH: 
		case node_type::POP:
			return size_store.create();
		case node_type::VARIABLE:
		case node_type::DYNAMIC_VARIABLE:
		case node_type::STATIC_OFFSET:
		case node_type::PARAM:
		case node_type::DYNAMIC_PARAM:
			return var_store.create();
		case node_type::RET:
			return return_data_store.create();
		default: return std::nullopt;
		}
	}

	namespace detail
	{
		template<> boolean& get_data<boolean>(ast& a, data_index_t i) { return a.constants.get<boolean>(i); }
		template<> string& get_data<string>(ast& a,data_index_t i) { return a.constants.get<string>(i); }
		template<> number& get_data<number>(ast& a,data_index_t i) { return a.constants.get<number>(i); }
		template<> function_data& get_data<function_data>(ast& a,data_index_t i) { return a.function_data_store.get_at(i); }
		template<> function_call_data& get_data<function_call_data>(ast& a,data_index_t i) { return a.function_call_data_store.get_at(i); }
		template<> label& get_data<label>(ast& a,data_index_t i) { return a.label_store.get_at(i); }
		template<> relative_offset& get_data<relative_offset>(ast& a,data_index_t i) { return a.relative_offset_store.get_at(i); }
		template<> stack_label& get_data<stack_label>(ast& a,data_index_t i) { return a.stack_label_store.get_at(i); }
		template<> size& get_data<size>(ast& a,data_index_t i) { return a.size_store.get_at(i); }
		template<> var_data& get_data<var_data>(ast& a,data_index_t i) { return a.var_store.get_at(i); }
		template<> return_data& get_data<return_data>(ast& a,data_index_t i) { return a.return_data_store.get_at(i); }
	}
}
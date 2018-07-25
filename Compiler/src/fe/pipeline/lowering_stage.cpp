#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"

namespace fe::ext_ast
{
	// Helper

	struct lowering_result
	{
		size_t allocated_stack_space;
		core_ast::node_id id;
	};

	struct lowering_context
	{
		using register_index = uint32_t;
		register_index next_register = 1;

		register_index alloc_register() { return next_register++; }
	};

	static bool has_children(node& n, ast& ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	static void link_child_parent(core_ast::node_id child, core_ast::node_id parent, core_ast::ast& new_ast)
	{
		new_ast.get_node(child).parent_id = parent;
		new_ast.get_node(parent).children.push_back(child);
	}

	lowering_result lower_assignment(node& n, ast& ext_ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto& children = ext_ast.children_of(n);
		assert(children.size() == 2);

		// a eval rhs -> result on stack
		// b get location of var
		// c move result on stack to var location
		// d dealloc from stack

		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		// a
		auto& value_node = ext_ast.get_node(children[1]);
		auto rhs = lower(value_node, ext_ast, new_ast, context);
		link_child_parent(rhs.id, block, new_ast);

		// b
		auto& id_node = ext_ast.get_node(children[0]);
		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto reg_index = ext_ast.get_data<ext_ast::identifier>(id_node.data_index).index_in_function;

		// c
		auto move = new_ast.create_node(core_ast::node_type::MOVE);
		auto& mv_data = new_ast.get_data<core_ast::move_data>(*new_ast.get_node(move).data_index);
		mv_data.from_t = core_ast::move_data::location_type::STACK;
		mv_data.to_t = core_ast::move_data::location_type::REG_DEREF;
		mv_data.from = -static_cast<int64_t>(rhs.allocated_stack_space);
		mv_data.to = reg_index;
		link_child_parent(move, block, new_ast);

		// d
		auto dealloc = new_ast.create_node(core_ast::node_type::SDEALLOC);
		link_child_parent(dealloc, block, new_ast);
		// #todo set size

		return {0, block};
	}

	lowering_result lower_tuple(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TUPLE);
		auto& children = ast.children_of(n);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE);

		size_t stack_size = 0;
		for (auto child : children)
		{
			auto res = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(res.id, tuple, new_ast);
			stack_size = res.allocated_stack_space;
		}

		return { tuple, stack_size };
	}

	lowering_result lower_block(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK);
		auto& children = ast.children_of(n);

		lowering_context nested_context = context;

		for (auto child : children)
		{
			auto res = lower(ast.get_node(child), ast, new_ast, nested_context);
			link_child_parent(res.id, block, new_ast);

			auto dealloc = new_ast.create_node(core_ast::node_type::SDEALLOC);
			link_child_parent(dealloc, block, new_ast);
			// #todo dealloc size
		}

		return { 0, block };
	}

	lowering_result lower_block_result(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		auto& child = ast.get_node(children[0]);
		return lower(child, ast, new_ast, context);
	}

	lowering_result lower_function(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION);

		// Parameters
		auto& param_node = ast.get_node(children[0]);
		if (param_node.kind == node_type::IDENTIFIER)
		{
			ast.get_data<ext_ast::identifier>(param_node.data_index).index_in_function = context.alloc_register();
			// #todo put stack offset in register
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"nyi");
			// #todo generate set each identifier of the tuple to value of the rhs
		}
		else throw std::runtime_error("Error: parameter node type incorrect");

		// Body
		auto& body_node = ast.get_node(children[1]);
		auto new_body = lower(body_node, ast, new_ast, context);
		link_child_parent(new_body.id, function_id, new_ast);

		// #todo functions should be root-scope only
		return { 0, function_id };
	}

	lowering_result lower_while_loop(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		auto new_while = new_ast.create_node(core_ast::node_type::WHILE_LOOP);
		
		auto test = lower(ast.get_node(children[0]), ast, new_ast, context);
		link_child_parent(test.id, new_while, new_ast);

		auto body = lower(ast.get_node(children[1]), ast, new_ast, context);
		link_child_parent(body.id, new_while, new_ast);

		return { 0, new_while };
	}

	lowering_result lower_if_statement(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		auto& children = ast.children_of(n);
		assert(children.size() >= 2);
		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

		bool contains_else = children.size() % 2 == 1;
		size_t size_excluding_else = children.size() - (children.size() % 2);
		for (int i = 0; i < size_excluding_else; i += 2)
		{
			auto test_id = lower(ast.get_node(children[i]), ast, new_ast, context);
			link_child_parent(test_id.id, branch, new_ast);

			auto body_id = lower(ast.get_node(children[i + 1]), ast, new_ast, context);
			link_child_parent(body_id.id, branch, new_ast);
		}

		if (contains_else)
		{
			// Create artificial 'test' node which is simply true
			auto tautology = new_ast.create_node(core_ast::node_type::BOOLEAN);
			auto& tautology_node = new_ast.get_node(tautology);
			assert(tautology_node.data_index);
			new_ast.get_data<boolean>(*tautology_node.data_index).value = true;
			link_child_parent(tautology_node.id, branch, new_ast);

			auto body_id = lower(ast.get_node(children.back()), ast, new_ast, context);
			link_child_parent(body_id.id, branch, new_ast);
		}

		return { 0, branch };
	}

	lowering_result lower_match(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MATCH);
		auto& children = ast.children_of(n);
		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

		for (auto i = 1; i < children.size(); i++)
		{
			auto& child_node = ast.get_node(children[i]);
			assert(child_node.kind == node_type::MATCH_BRANCH);
			assert(children.size() == 2);

			auto& child_children = ast.children_of(child_node);
			auto test = lower(ast.get_node(child_children[0]), ast, new_ast, context);
			link_child_parent(test.id, branch, new_ast);

			auto body = lower(ast.get_node(child_children[1]), ast, new_ast, context);
			link_child_parent(body.id, branch, new_ast);
		}

		return { 0, branch };
	}

	lowering_result lower_id(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(!has_children(n, ast));
		auto& data = ast.get_data<identifier>(n.data_index);

		auto block = new_ast.create_node(core_ast::node_type::BLOCK);
		auto alloc = new_ast.create_node(core_ast::node_type::SALLOC);
		link_child_parent(alloc, block, new_ast);
		// #todo alloc size of variable
		auto read = new_ast.create_node(core_ast::node_type::MOVE);
		link_child_parent(read, block, new_ast);
		// #todo move from stack to top of stack
		return { /* #todo sizeof var */ 0, block };
	}

	lowering_result lower_string(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		auto& str_data = ast.get_data<string>(n.data_index);

		// #todo do something?
		auto str = new_ast.create_node(core_ast::node_type::STRING);
		auto& str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		return { /* #todo sizeof str*/ 0, str };
	}

	lowering_result lower_boolean(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));
		auto& bool_data = ast.get_data<boolean>(n.data_index);

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN);
		auto& bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return { 1, bool_id };
	}

	lowering_result lower_number(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		auto& num_data = ast.get_data<number>(n.data_index);

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER);
		auto& num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		if (num_data.type == number_type::I32 || num_data.type == number_type::UI32)
			return { 4, num_id };
		else if (num_data.type == number_type::I64 || num_data.type == number_type::UI64)
			return { 8, num_id };
	}

	lowering_result lower_function_call(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);

		auto id = lower(ast.get_node(children[0]), ast, new_ast, context);
		auto& id_node = new_ast.get_node(id.id);
		link_child_parent(id.id, fun_id, new_ast);

		auto param = lower(ast.get_node(children[1]), ast, new_ast, context);
		auto& param_node = new_ast.get_node(param.id);
		link_child_parent(param.id, fun_id, new_ast);

		return {  fun_id };
	}

	lowering_result lower_module_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(ast.children_of(n).size() == 1);
		return { 0, new_ast.create_node(core_ast::node_type::NOP) };
	}

	lowering_result lower_import_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return { 0, new_ast.create_node(core_ast::node_type::NOP) };
	}

	lowering_result lower_export_stmt(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return { 0,new_ast.create_node(core_ast::node_type::NOP) };
	}

	lowering_result lower_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::DECLARATION);
		auto& children = ast.children_of(n);
		assert(children.size() == 3);

		auto dec = new_ast.create_node(core_ast::node_type::MOVE);

		auto& lhs_node = ast.get_node(children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			ast.get_data<ext_ast::identifier>(lhs_node.data_index).index_in_function = context.alloc_register();
			// #todo put stack offset in register
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"identifier tuple lowering nyi");
		}

		auto rhs = lower(ast.get_node(children[2]), ast, new_ast, context);
		link_child_parent(rhs.id, dec, new_ast);

		return { 0, dec };
	}

	lowering_result lower_record(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::RECORD);
		return { 0, new_ast.create_node(core_ast::node_type::NOP) };
	}

	lowering_result lower_type_definition(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fn = new_ast.create_node(core_ast::node_type::FUNCTION);
		// #todo set function properties

		auto ret = new_ast.create_node(core_ast::node_type::RET);
		link_child_parent(ret, fn, new_ast);

		return { 0, fn };
	}

	lowering_result lower_type_atom(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		return { 0, new_ast.create_node(core_ast::node_type::NOP) };
	}

	lowering_result lower_reference(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::REFERENCE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		assert(!"nyi");
		return { 0, 0 };
	}

	lowering_result lower_array_value(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.data_index == no_data);

		auto arr = new_ast.create_node(core_ast::node_type::TUPLE);
		auto& children = ast.children_of(n);
		size_t size_sum = 0;
		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(new_child.id, arr, new_ast);
			size_sum += new_child.allocated_stack_space;
		}

		return { size_sum, arr };
	}

	lowering_result lower_binary_op(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		assert(n.data_index != no_data);

		assert(!"nyi");

		return { 0, 0 };
	}

	lowering_result lower(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:         return lower_assignment(n, ast, new_ast, context);           break;
		case node_type::TUPLE:              return lower_tuple(n, ast, new_ast, context);                break;
		case node_type::BLOCK:              return lower_block(n, ast, new_ast, context);                break;
		case node_type::BLOCK_RESULT:       return lower_block_result(n, ast, new_ast, context);         break;
		case node_type::FUNCTION:           return lower_function(n, ast, new_ast, context);             break;
		case node_type::WHILE_LOOP:         return lower_while_loop(n, ast, new_ast, context);           break;
		case node_type::IF_STATEMENT:       return lower_if_statement(n, ast, new_ast, context);         break;
		case node_type::MATCH:              return lower_match(n, ast, new_ast, context);                break;
		case node_type::IDENTIFIER:         return lower_id(n, ast, new_ast, context);                   break;
		case node_type::STRING:             return lower_string(n, ast, new_ast, context);               break;
		case node_type::BOOLEAN:            return lower_boolean(n, ast, new_ast, context);              break;
		case node_type::NUMBER:             return lower_number(n, ast, new_ast, context);               break;
		case node_type::FUNCTION_CALL:      return lower_function_call(n, ast, new_ast, context);        break;
		case node_type::MODULE_DECLARATION: return lower_module_declaration(n, ast, new_ast, context);   break;
		case node_type::IMPORT_DECLARATION: return lower_import_declaration(n, ast, new_ast, context);   break;
		case node_type::EXPORT_STMT:        return lower_export_stmt(n, ast, new_ast, context);          break;
		case node_type::DECLARATION:        return lower_declaration(n, ast, new_ast, context);          break;
		case node_type::RECORD:             return lower_record(n, ast, new_ast, context);               break;
		case node_type::TYPE_DEFINITION:    return lower_type_definition(n, ast, new_ast, context);      break;
		case node_type::TYPE_ATOM:          return lower_type_atom(n, ast, new_ast, context);            break;
		case node_type::REFERENCE:          return lower_reference(n, ast, new_ast, context);            break;
		case node_type::ARRAY_VALUE:        return lower_array_value(n, ast, new_ast, context);          break;
		default:
			if (ext_ast::is_binary_op(n.kind)) return lower_binary_op(n, ast, new_ast, context);

			assert(!"Node type not lowerable");
			throw std::runtime_error("Fatal Error - Node type not lowerable");
		}
	}


	core_ast::ast lower(ast& ast)
	{
		auto& root = ast.get_node(ast.root_id());
		assert(root.kind == node_type::BLOCK);
		auto& children = ast.children_of(root);

		lowering_context context;
		core_ast::ast new_ast(core_ast::node_type::BLOCK);
		auto block = new_ast.root_id();

		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(new_child.id, block, new_ast);
		}

		return new_ast;
	}
}
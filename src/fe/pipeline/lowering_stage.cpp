#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"

namespace fe::ext_ast
{
	core_ast::node_id lower_assignment(node& n, ast& ext_ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		assert(n.children.size() == 2);

		auto& id_node = ext_ast.get_node(n.children[0]);
		auto lhs = lower(id_node, ext_ast, new_ast);
		auto& value_node = ext_ast.get_node(n.children[1]);
		auto rhs = lower(value_node, ext_ast, new_ast);

		auto set = new_ast.create_node(core_ast::node_type::SET);
		new_ast.get_node(set).children.push_back(lhs);
		new_ast.get_node(set).children.push_back(rhs);
		new_ast.get_node(lhs).parent_id = set;
		new_ast.get_node(rhs).parent_id = set;
		return set;
	}

	core_ast::node_id lower_tuple(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TUPLE);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE);
		auto& tuple_node = new_ast.get_node(tuple);

		for (auto child : n.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.get_node(new_child).parent_id = tuple;
			tuple_node.children.push_back(new_child);
		}

		return tuple;
	}

	core_ast::node_id lower_block(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK);
		auto& block_node = new_ast.get_node(block);

		for (auto child : n.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			block_node.children.push_back(new_child);
			new_ast.get_node(new_child).parent_id = block;
		}
		return block;
	}

	core_ast::node_id lower_block_result(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		assert(n.children.size() == 1);
		auto& child = ast.get_node(n.children[0]);
		return lower(child, ast, new_ast);
	}

	core_ast::node_id lower_function(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::FUNCTION);
		assert(n.children.size() == 4);
		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION);
		auto& function_node = new_ast.get_node(function_id);

		// Name
		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.kind == node_type::IDENTIFIER);
		auto new_id_node = lower(id_node, ast, new_ast);
		new_ast.get_node(new_id_node).parent_id = function_id;
		function_node.children.push_back(new_id_node);

		// Parameters
		auto& from_node = ast.get_node(n.children[1]);
		if (from_node.kind == node_type::TUPLE_DECLARATION)
		{
			auto param_id = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);
			auto& param_node = new_ast.get_node(param_id);
			function_node.children.push_back(param_id);
			param_node.parent_id = function_id;

			for (auto child : from_node.children)
			{
				auto& child_node = ast.get_node(child);
				assert(child_node.kind == node_type::ATOM_DECLARATION);
				auto& child_id_node = ast.get_node(child_node.children[1]);
				assert(child_id_node.kind == node_type::IDENTIFIER);

				auto child_id = lower(child_node, ast, new_ast);
				new_ast.get_node(child_id).parent_id = param_id;
				param_node.children.push_back(child_id);
			}
		}
		else if (from_node.kind == node_type::ATOM_DECLARATION)
		{
			auto& param_node = ast.get_node(from_node.children[1]);
			assert(param_node.kind == node_type::IDENTIFIER);
			auto param_id = lower(param_node, ast, new_ast);
			new_ast.get_node(param_id).parent_id = function_id;
			function_node.children.push_back(param_id);
		}
		else throw std::runtime_error("Error: parameter node type incorrect");

		// Body
		auto& body_node = ast.get_node(n.children[3]);
		auto new_body = lower(body_node, ast, new_ast);
		new_ast.get_node(new_body).parent_id = function_id;
		function_node.children.push_back(new_body);

		return function_id;
	}

	core_ast::node_id lower_while_loop(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);
		auto new_while = new_ast.create_node(core_ast::node_type::WHILE_LOOP);
		auto& while_node = new_ast.get_node(new_while);
		
		auto test_id = lower(ast.get_node(n.children[0]), ast, new_ast);
		new_ast.get_node(test_id).parent_id = new_while;
		while_node.children.push_back(test_id);

		auto body_id = lower(ast.get_node(n.children[1]), ast, new_ast);
		new_ast.get_node(body_id).parent_id = new_while;
		while_node.children.push_back(body_id);

		return new_while;
	}

	core_ast::node_id lower_if_statement(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		assert(n.children.size() == 2);
		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);
		auto& branch_node = new_ast.get_node(branch);
		
		auto test_id = lower(ast.get_node(n.children[0]), ast, new_ast);
		new_ast.get_node(test_id).parent_id = branch;
		branch_node.children.push_back(test_id);

		auto body_id = lower(ast.get_node(n.children[1]), ast, new_ast);
		new_ast.get_node(body_id).parent_id = branch;
		branch_node.children.push_back(body_id);

		return branch;
	}

	core_ast::node_id lower_match(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::MATCH);
		assert(n.children.size() % 2 == 0);

		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);
		auto& branch_node = new_ast.get_node(branch);

		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			assert(child_node.kind == node_type::MATCH_BRANCH);
			assert(child_node.children.size() == 2);
			auto test = lower(ast.get_node(child_node.children[0]), ast, new_ast);
			auto body = lower(ast.get_node(child_node.children[1]), ast, new_ast);

			branch_node.children.push_back(test);
			new_ast.get_node(test).parent_id = branch;
			branch_node.children.push_back(body);
			new_ast.get_node(body).parent_id = branch;
		}

		return branch;
	}

	core_ast::node_id lower_id(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(n.children.size() == 0);
		auto& data = ast.get_data<identifier>(n.data_index.value());

		auto modules = std::vector<std::string>(
			data.segments.begin(),
			data.segments.end() - 1 - data.offsets.size()
			);
		std::string name = *(data.segments.end() - 1 - data.offsets.size());
		// If the scope distance is not defined then this id is the lhs of a declaration
		auto scope_distance = data.scope_distance ? *data.scope_distance : 0;

		auto id = new_ast.create_node(core_ast::node_type::IDENTIFIER);
		auto& id_node = new_ast.get_node(id);
		new_ast.get_data<core_ast::identifier>(*id_node.data_index) = core_ast::identifier(modules, name, scope_distance, data.offsets);

		return id;
	}

	core_ast::node_id lower_string(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::STRING);
		assert(n.children.size() == 0);
		auto& str_data = ast.get_data<string>(n.data_index.value());

		auto str = new_ast.create_node(core_ast::node_type::STRING);
		auto& str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		return str;
	}

	core_ast::node_id lower_boolean(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(n.children.size() == 0);
		auto& bool_data = ast.get_data<boolean>(n.data_index.value());

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN);
		auto& bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return bool_id;
	}

	core_ast::node_id lower_number(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(n.children.size() == 0);
		auto& num_data = ast.get_data<number>(n.data_index.value());

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER);
		auto& num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		return num_id;
	}

	core_ast::node_id lower_function_call(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);
		auto& fun_node = new_ast.get_node(fun_id);

		auto id_id = lower(ast.get_node(n.children[0]), ast, new_ast);
		auto& id_node = new_ast.get_node(id_id);
		id_node.parent_id = fun_id;

		auto param_id = lower(ast.get_node(n.children[1]), ast, new_ast);
		auto& param_node = new_ast.get_node(param_id);
		param_node.parent_id = fun_id;

		fun_node.children.push_back(id_id);
		fun_node.children.push_back(param_id);

		return fun_id;
	}

	core_ast::node_id lower_module_declaration(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(n.children.size() == 1);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_import_declaration(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_export_stmt(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_declaration(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::DECLARATION);
		assert(n.children.size() == 3);

		auto dec = new_ast.create_node(core_ast::node_type::SET);
		auto& dec_node = new_ast.get_node(dec);

		auto& lhs_node = ast.get_node(n.children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			auto lhs = lower(lhs_node, ast, new_ast);
			new_ast.get_node(lhs).parent_id = dec;
			dec_node.children.push_back(lhs);
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto lhs = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);
			auto& new_lhs_node = new_ast.get_node(lhs);
			new_lhs_node.parent_id = dec;

			for (auto child : lhs_node.children)
			{
				auto new_child_id = lower(ast.get_node(child), ast, new_ast);
				auto& new_child_node = new_ast.get_node(new_child_id);
				new_child_node.parent_id = lhs;
				new_lhs_node.children.push_back(new_child_id);
			}

			dec_node.children.push_back(lhs);
		}

		auto rhs = lower(ast.get_node(n.children[2]), ast, new_ast);
		new_ast.get_node(rhs).parent_id = dec;
		dec_node.children.push_back(rhs);

		return dec;
	}

	core_ast::node_id lower_tuple_declaration(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TUPLE_DECLARATION);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_type_definition(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		assert(n.children.size() == 2);

		auto set = new_ast.create_node(core_ast::node_type::SET);
		auto& set_node = new_ast.get_node(set);

		auto lhs = lower(ast.get_node(n.children[0]), ast, new_ast);
		auto& lhs_node = new_ast.get_node(lhs);
		assert(lhs_node.kind == core_ast::node_type::IDENTIFIER);
		auto& lhs_data = new_ast.get_data<core_ast::identifier>(*lhs_node.data_index);
		lhs_node.parent_id = set;
		set_node.children.push_back(lhs);

		auto fn_rhs = new_ast.create_node(core_ast::node_type::FUNCTION);
		auto& fn_node = new_ast.get_node(fn_rhs);
		fn_node.parent_id = set;
		set_node.children.push_back(fn_rhs);

		{
			auto fn_name = new_ast.create_node(core_ast::node_type::IDENTIFIER);
			auto& name_node = new_ast.get_node(fn_name);
			auto& name_data = new_ast.get_data<core_ast::identifier>(*name_node.data_index);
			name_data.variable_name = lhs_data.variable_name;
			name_node.parent_id = fn_rhs;

			fn_node.children.push_back(fn_name);

			auto param = new_ast.create_node(core_ast::node_type::IDENTIFIER);
			auto& param_node = new_ast.get_node(param);
			auto& param_data = new_ast.get_data<core_ast::identifier>(*param_node.data_index);
			param_data.variable_name = "_arg0";
			param_node.parent_id = fn_rhs;

			// Push twice, for parameter and body
			fn_node.children.push_back(param);
			fn_node.children.push_back(param);
		}

		return set;
	}

	core_ast::node_id lower_type_atom(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_reference(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::REFERENCE);
		assert(n.children.size() == 1);
		auto ref = new_ast.create_node(core_ast::node_type::REFERENCE);
		auto child = lower(ast.get_node(n.children[0]), ast, new_ast);
		new_ast.get_node(child).parent_id = ref;
		new_ast.get_node(ref).children.push_back(ref);
		return ref;
	}

	core_ast::node_id lower_array_value(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(!n.data_index.has_value());
		auto arr = new_ast.create_node(core_ast::node_type::TUPLE);

		for (auto child : n.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.get_node(new_child).parent_id = arr;
			new_ast.get_node(arr).children.push_back(new_child);
		}

		return arr;
	}

	core_ast::node_id lower_binary_op(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(ext_ast::is_binary_op(n.kind));
		assert(n.children.size() == 2);
		assert(n.data_index);

		auto fun_call = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);
		auto& fun_call_node = new_ast.get_node(fun_call);

		auto fun_name_id = new_ast.create_node(core_ast::node_type::IDENTIFIER);
		auto& fun_name_node = new_ast.get_node(fun_name_id);
		auto& name_data = new_ast.get_data<core_ast::identifier>(*fun_name_node.data_index);
		name_data.modules = { "_core" };
		name_data.variable_name = ast.get_data<string>(*n.data_index).value;
		name_data.offsets = {};
		assert(n.name_scope_id);
		name_data.scope_distance = ast.get_name_scope(*n.name_scope_id).depth();

		fun_call_node.children.push_back(fun_name_id);
		fun_name_node.parent_id = fun_call;

		auto param_tuple = new_ast.create_node(core_ast::node_type::TUPLE);
		auto& param_tuple_node = new_ast.get_node(param_tuple);

		for (auto child : n.children)
		{
			auto new_child_id = lower(ast.get_node(child), ast, new_ast);
			auto& new_child_node = new_ast.get_node(new_child_id);
			new_child_node.parent_id = param_tuple;
			param_tuple_node.children.push_back(new_child_id);
		}

		fun_call_node.children.push_back(param_tuple);
		param_tuple_node.parent_id = fun_call;

		return fun_call;
	}

	core_ast::node_id lower(node& n, ast& ast, core_ast::ast& new_ast)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:         return lower_assignment(n, ast, new_ast);           break;
		case node_type::TUPLE:              return lower_tuple(n, ast, new_ast);                break;
		case node_type::BLOCK:              return lower_block(n, ast, new_ast);                break;
		case node_type::BLOCK_RESULT:       return lower_block_result(n, ast, new_ast);         break;
		case node_type::FUNCTION:           return lower_function(n, ast, new_ast);             break;
		case node_type::WHILE_LOOP:         return lower_while_loop(n, ast, new_ast);           break;
		case node_type::IF_STATEMENT:       return lower_if_statement(n, ast, new_ast);         break;
		case node_type::MATCH:              return lower_match(n, ast, new_ast);                break;
		case node_type::IDENTIFIER:         return lower_id(n, ast, new_ast);                   break;
		case node_type::STRING:             return lower_string(n, ast, new_ast);               break;
		case node_type::BOOLEAN:            return lower_boolean(n, ast, new_ast);              break;
		case node_type::NUMBER:             return lower_number(n, ast, new_ast);               break;
		case node_type::FUNCTION_CALL:      return lower_function_call(n, ast, new_ast);        break;
		case node_type::MODULE_DECLARATION: return lower_module_declaration(n, ast, new_ast);   break;
		case node_type::IMPORT_DECLARATION: return lower_import_declaration(n, ast, new_ast);   break;
		case node_type::EXPORT_STMT:        return lower_export_stmt(n, ast, new_ast);          break;
		case node_type::DECLARATION:        return lower_declaration(n, ast, new_ast);          break;
		case node_type::TUPLE_DECLARATION:  return lower_tuple_declaration(n, ast, new_ast);    break;
		case node_type::TYPE_DEFINITION:    return lower_type_definition(n, ast, new_ast);      break;
		case node_type::TYPE_ATOM:          return lower_type_atom(n, ast, new_ast);            break;
		case node_type::REFERENCE:          return lower_reference(n, ast, new_ast);            break;
		case node_type::ARRAY_VALUE:        return lower_array_value(n, ast, new_ast);          break;
		default:
			if (ext_ast::is_binary_op(n.kind)) return lower_binary_op(n, ast, new_ast);

			assert(!"Node type not lowerable");
			throw std::runtime_error("Fatal Error - Node type not lowerable");
		}
	}


	core_ast::ast lower(ast& ast)
	{
		auto& root = ast.get_node(ast.root_id());
		assert(root.kind == node_type::BLOCK);

		core_ast::ast new_ast(core_ast::node_type::BLOCK);
		auto block = new_ast.root_id();
		auto& block_node = new_ast.get_node(block);

		for (auto child : root.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			block_node.children.push_back(new_child);
			new_ast.get_node(new_child).parent_id = block;
		}

		return new_ast;
	}
}
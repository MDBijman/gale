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

		for (auto child : n.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.get_node(new_child).parent_id = tuple;
			new_ast.get_node(tuple).children.push_back(new_child);
		}

		return tuple;
	}

	core_ast::node_id lower_block(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		for (auto child : n.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.get_node(block).children.push_back(new_child);
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
		assert(n.children.size() == 2);
		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION);

		// Parameters
		auto& param_node = ast.get_node(n.children[0]);
		if (param_node.kind == node_type::IDENTIFIER)
		{
			auto param_id = lower(param_node, ast, new_ast);
			new_ast.get_node(param_id).parent_id = function_id;
			new_ast.get_node(function_id).children.push_back(param_id);
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto param_id = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);
			auto& new_param_node = new_ast.get_node(param_id);
			new_ast.get_node(function_id).children.push_back(param_id);
			new_param_node.parent_id = function_id;

			for (auto child : param_node.children)
			{
				auto& child_node = ast.get_node(child);
				assert(child_node.kind == node_type::RECORD_ELEMENT);
				auto& child_id_node = ast.get_node(child_node.children[1]);
				assert(child_id_node.kind == node_type::IDENTIFIER);

				auto child_id = lower(child_node, ast, new_ast);
				new_ast.get_node(child_id).parent_id = param_id;
				new_param_node.children.push_back(child_id);
			}
		}
		else throw std::runtime_error("Error: parameter node type incorrect");

		// Body
		auto& body_node = ast.get_node(n.children[1]);
		auto new_body = lower(body_node, ast, new_ast);
		new_ast.get_node(new_body).parent_id = function_id;
		new_ast.get_node(function_id).children.push_back(new_body);

		return function_id;
	}

	core_ast::node_id lower_while_loop(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);
		auto new_while = new_ast.create_node(core_ast::node_type::WHILE_LOOP);
		
		auto test_id = lower(ast.get_node(n.children[0]), ast, new_ast);
		new_ast.get_node(test_id).parent_id = new_while;

		auto body_id = lower(ast.get_node(n.children[1]), ast, new_ast);
		new_ast.get_node(body_id).parent_id = new_while;

		auto& while_node = new_ast.get_node(new_while);
		while_node.children.push_back(test_id);
		while_node.children.push_back(body_id);

		return new_while;
	}

	core_ast::node_id lower_if_statement(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		assert(n.children.size() >= 2);
		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

		bool contains_else = n.children.size() % 2 == 1 ? true : false;
		size_t size_excluding_else = n.children.size() - (n.children.size() % 2);
		for (int i = 0; i < size_excluding_else; i += 2)
		{
			auto test_id = lower(ast.get_node(n.children[i]), ast, new_ast);
			new_ast.get_node(test_id).parent_id = branch;

			auto body_id = lower(ast.get_node(n.children[i + 1]), ast, new_ast);
			new_ast.get_node(body_id).parent_id = branch;

			auto& branch_node = new_ast.get_node(branch);
			branch_node.children.push_back(test_id);
			branch_node.children.push_back(body_id);
		}

		if (contains_else)
		{
			// Create artificial 'test' node which is simply true
			auto tautology = new_ast.create_node(core_ast::node_type::BOOLEAN);
			auto& tautology_node = new_ast.get_node(tautology);
			assert(tautology_node.data_index);
			new_ast.get_data<boolean>(*tautology_node.data_index).value = true;
			tautology_node.parent_id = branch;

			auto body_id = lower(ast.get_node(n.children.back()), ast, new_ast);
			new_ast.get_node(body_id).parent_id = branch;

			auto& branch_node = new_ast.get_node(branch);
			branch_node.children.push_back(tautology);
			branch_node.children.push_back(body_id);
		}

		return branch;
	}

	core_ast::node_id lower_match(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::MATCH);

		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

		for (auto i = 1; i < n.children.size(); i++)
		{
			auto& child_node = ast.get_node(n.children[i]);
			assert(child_node.kind == node_type::MATCH_BRANCH);
			assert(child_node.children.size() == 2);

			auto test = lower(ast.get_node(child_node.children[0]), ast, new_ast);
			new_ast.get_node(test).parent_id = branch;

			auto body = lower(ast.get_node(child_node.children[1]), ast, new_ast);
			new_ast.get_node(body).parent_id = branch;

			auto& branch_node = new_ast.get_node(branch);
			branch_node.children.push_back(test);
			branch_node.children.push_back(body);
		}

		return branch;
	}

	core_ast::node_id lower_id(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(n.children.size() == 0);
		auto& data = ast.get_data<identifier>(n.data_index.value());

		auto modules = std::vector<std::string_view>(
			data.segments.begin(),
			data.segments.end() - 1 - data.offsets.size()
			);
		std::string_view name = *(data.segments.end() - 1 - data.offsets.size());
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

		auto id_id = lower(ast.get_node(n.children[0]), ast, new_ast);
		auto& id_node = new_ast.get_node(id_id);
		id_node.parent_id = fun_id;

		auto param_id = lower(ast.get_node(n.children[1]), ast, new_ast);
		auto& param_node = new_ast.get_node(param_id);
		param_node.parent_id = fun_id;

		auto& fun_node = new_ast.get_node(fun_id);
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

		auto& lhs_node = ast.get_node(n.children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			auto lhs = lower(lhs_node, ast, new_ast);
			new_ast.get_node(lhs).parent_id = dec;
			new_ast.get_node(dec).children.push_back(lhs);
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto lhs = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);
			new_ast.get_node(lhs).parent_id = dec;

			for (auto child : lhs_node.children)
			{
				auto new_child_id = lower(ast.get_node(child), ast, new_ast);
				auto& new_child_node = new_ast.get_node(new_child_id);
				new_child_node.parent_id = lhs;
				new_ast.get_node(lhs).children.push_back(new_child_id);
			}

			new_ast.get_node(dec).children.push_back(lhs);
		}

		auto rhs = lower(ast.get_node(n.children[2]), ast, new_ast);
		new_ast.get_node(rhs).parent_id = dec;
		new_ast.get_node(dec).children.push_back(rhs);

		return dec;
	}

	core_ast::node_id lower_record(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::RECORD);
		return new_ast.create_node(core_ast::node_type::NOP);
	}

	core_ast::node_id lower_type_definition(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		assert(n.children.size() == 2);

		auto set = new_ast.create_node(core_ast::node_type::SET);

		auto lhs = lower(ast.get_node(n.children[0]), ast, new_ast);
		auto& lhs_node = new_ast.get_node(lhs);
		assert(lhs_node.kind == core_ast::node_type::IDENTIFIER);
		lhs_node.parent_id = set;

		auto fn_rhs = new_ast.create_node(core_ast::node_type::FUNCTION);
		auto& fn_node = new_ast.get_node(fn_rhs);
		fn_node.parent_id = set;

		auto& set_node = new_ast.get_node(set);
		set_node.children.push_back(lhs);
		set_node.children.push_back(fn_rhs);

		{
			auto param = new_ast.create_node(core_ast::node_type::IDENTIFIER);
			auto& param_node = new_ast.get_node(param);
			auto& param_data = new_ast.get_data<core_ast::identifier>(*param_node.data_index);
			param_data.variable_name = "_arg0";
			param_node.parent_id = fn_rhs;

			// Push twice, for parameter and body
			new_ast.get_node(fn_rhs).children.push_back(param);
			new_ast.get_node(fn_rhs).children.push_back(param);
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

		/*
			Logical operators require special semantics due to short circuiting, so we create explicit branches
			that implement this.

			'x && y' becomes 'if (!x) { false } elseif (y) { true } else { false }'
			'x || y' becomes 'if (x) { true } elseif (y) { true } else { false }'

			Other operators are lowered into a function call to the corresponding _core function.

			For performance reasons it might make sense to create 'native' core nodes for all/some operations.

			MB 30-5-2018
		*/

		if (n.kind == node_type::AND)
		{
			auto branch = new_ast.create_node(core_ast::node_type::BRANCH);
			auto first_test = new_ast.create_node(core_ast::node_type::FUNCTION_CALL, branch);
			{
				auto fun_name = new_ast.create_node(core_ast::node_type::IDENTIFIER, first_test);
				new_ast.get_data<core_ast::identifier>(*new_ast.get_node(fun_name).data_index) = core_ast::identifier(
					{ "_core" }, "not std.bool -> std.bool",
					ast.get_name_scope(*n.name_scope_id).depth(ast.name_scope_cb()), {});

				auto fun_arg = lower(ast.get_node(n.children[0]), ast, new_ast);
				new_ast.get_node(fun_arg).parent_id = first_test;

				new_ast.get_node(first_test).children.push_back(fun_name);
				new_ast.get_node(first_test).children.push_back(fun_arg);
			}
			auto first_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(first_body).data_index).value = false;

			auto second_test = lower(ast.get_node(n.children[1]), ast, new_ast);
			new_ast.get_node(second_test).parent_id = branch;
			auto second_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(second_body).data_index).value = true;

			auto else_test = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(else_test).data_index).value = true;
			auto else_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(else_body).data_index).value = false;

			new_ast.get_node(branch).children.push_back(first_test);
			new_ast.get_node(branch).children.push_back(first_body);
			new_ast.get_node(branch).children.push_back(second_test);
			new_ast.get_node(branch).children.push_back(second_body);
			new_ast.get_node(branch).children.push_back(else_test);
			new_ast.get_node(branch).children.push_back(else_body);
			return branch;
		}
		else if (n.kind == node_type::OR)
		{
			auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

			auto first_test = lower(ast.get_node(n.children[0]), ast, new_ast);
			new_ast.get_node(first_test).parent_id = branch;
			auto first_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(first_body).data_index).value = true;

			auto second_test = lower(ast.get_node(n.children[1]), ast, new_ast);
			new_ast.get_node(second_test).parent_id = branch;
			auto second_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(second_body).data_index).value = true;

			auto else_test = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(else_test).data_index).value = true;
			auto else_body = new_ast.create_node(core_ast::node_type::BOOLEAN, branch);
			new_ast.get_data<boolean>(*new_ast.get_node(else_body).data_index).value = false;

			new_ast.get_node(branch).children.push_back(first_test);
			new_ast.get_node(branch).children.push_back(first_body);
			new_ast.get_node(branch).children.push_back(second_test);
			new_ast.get_node(branch).children.push_back(second_body);
			new_ast.get_node(branch).children.push_back(else_test);
			new_ast.get_node(branch).children.push_back(else_body);
			return branch;
		}
		else
		{
			auto fun_call = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);

			auto fun_name_id = new_ast.create_node(core_ast::node_type::IDENTIFIER);
			auto& name_data = new_ast.get_data<core_ast::identifier>(*new_ast.get_node(fun_name_id).data_index);
			name_data.modules = { "_core" };
			name_data.variable_name = ast.get_data<string>(*n.data_index).value;
			name_data.offsets = {};
			assert(n.name_scope_id);
			name_data.scope_distance = ast.get_name_scope(*n.name_scope_id).depth(ast.name_scope_cb());

			new_ast.get_node(fun_call).children.push_back(fun_name_id);
			new_ast.get_node(fun_name_id).parent_id = fun_call;

			auto param_tuple = new_ast.create_node(core_ast::node_type::TUPLE);

			for (auto child : n.children)
			{
				auto new_child_id = lower(ast.get_node(child), ast, new_ast);
				auto& new_child_node = new_ast.get_node(new_child_id);
				new_child_node.parent_id = param_tuple;
				new_ast.get_node(param_tuple).children.push_back(new_child_id);
			}

			new_ast.get_node(fun_call).children.push_back(param_tuple);
			new_ast.get_node(param_tuple).parent_id = fun_call;

			return fun_call;
		}
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
		case node_type::RECORD:             return lower_record(n, ast, new_ast);               break;
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

		for (auto child : root.children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.get_node(block).children.push_back(new_child);
			new_ast.get_node(new_child).parent_id = block;
		}

		return new_ast;
	}
}
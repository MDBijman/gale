#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/data/types.h"

namespace fe::ext_ast
{
	// Helper

	static bool has_children(node& n, ast& ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	core_ast::node_id lower_assignment(node& n, ast& ext_ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto& children = ext_ast.children_of(n);
		assert(children.size() == 2);

		auto set = new_ast.create_node(core_ast::node_type::SET);

		auto& id_node = ext_ast.get_node(children[0]);
		auto lhs = lower(id_node, ext_ast, new_ast);
		new_ast.get_node(lhs).size = (*ext_ast
			.get_type_scope(id_node.type_scope_id)
			.resolve_type(ext_ast.get_data<identifier>(id_node.data_index), ext_ast.type_scope_cb()))
			.type.calculate_size();
		new_ast.link_child_parent(lhs, set);

		auto& value_node = ext_ast.get_node(children[1]);
		auto rhs = lower(value_node, ext_ast, new_ast);
		new_ast.link_child_parent(rhs, set);

		return set;
	}

	core_ast::node_id lower_tuple(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::TUPLE);
		auto& children = ast.children_of(n);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE);

		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.link_child_parent(new_child, tuple);
		}

		return tuple;
	}

	core_ast::node_id lower_block(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		auto& children = ast.children_of(n);
		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.link_child_parent(new_child, block);
		}
		return block;
	}

	core_ast::node_id lower_block_result(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		auto& child = ast.get_node(children[0]);
		return lower(child, ast, new_ast);
	}

	core_ast::node_id lower_function(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::FUNCTION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION);

		// Parameters
		auto& param_node = ast.get_node(children[0]);
		if (param_node.kind == node_type::IDENTIFIER)
		{
			auto param_id = lower(param_node, ast, new_ast);
			new_ast.link_child_parent(param_id, function_id);

			new_ast.get_node(param_id).size = (*ast
				.get_type_scope(param_node.type_scope_id)
				.resolve_variable(ast.get_data<identifier>(param_node.data_index), ast.type_scope_cb()))
				.type.calculate_size();
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto param_id = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);
			new_ast.link_child_parent(param_id, function_id);

			auto& param_children = ast.children_of(param_node);
			for (auto child : param_children)
			{
				auto& child_node = ast.get_node(child);
				auto& param_child_children = ast.children_of(child_node);
				assert(child_node.kind == node_type::RECORD_ELEMENT);
				auto& child_id_node = ast.get_node(param_child_children[1]);
				assert(child_id_node.kind == node_type::IDENTIFIER);

				auto child_id = lower(child_node, ast, new_ast);
				new_ast.link_child_parent(child_id, param_id);

				new_ast.get_node(child_id).size = (*ast
					.get_type_scope(child_id_node.type_scope_id)
					.resolve_variable(ast.get_data<identifier>(child_id_node.data_index), ast.type_scope_cb()))
					.type.calculate_size();
			}
		}
		else throw std::runtime_error("Error: parameter node type incorrect");

		// Body
		auto& body_node = ast.get_node(children[1]);
		auto new_body = lower(body_node, ast, new_ast);
		new_ast.link_child_parent(new_body, function_id);

		return function_id;
	}

	core_ast::node_id lower_while_loop(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		auto new_while = new_ast.create_node(core_ast::node_type::WHILE_LOOP);

		auto test_id = lower(ast.get_node(children[0]), ast, new_ast);
		new_ast.link_child_parent(test_id, new_while);

		auto body_id = lower(ast.get_node(children[1]), ast, new_ast);
		new_ast.link_child_parent(body_id, new_while);

		return new_while;
	}

	core_ast::node_id lower_if_statement(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		auto& children = ast.children_of(n);
		assert(children.size() >= 2);
		auto branch = new_ast.create_node(core_ast::node_type::BRANCH);

		bool contains_else = children.size() % 2 == 1;
		size_t size_excluding_else = children.size() - (children.size() % 2);
		for (int i = 0; i < size_excluding_else; i += 2)
		{
			auto test_id = lower(ast.get_node(children[i]), ast, new_ast);
			new_ast.link_child_parent(test_id, branch);

			auto body_id = lower(ast.get_node(children[i + 1]), ast, new_ast);
			new_ast.link_child_parent(body_id, branch);
		}

		if (contains_else)
		{
			// Create artificial 'test' node which is simply true
			auto tautology = new_ast.create_node(core_ast::node_type::BOOLEAN);
			auto& tautology_node = new_ast.get_node(tautology);
			assert(tautology_node.data_index);
			new_ast.get_data<boolean>(*tautology_node.data_index).value = true;
			new_ast.link_child_parent(tautology, branch);

			auto body_id = lower(ast.get_node(children.back()), ast, new_ast);
			new_ast.link_child_parent(body_id, branch);
		}

		return branch;
	}

	core_ast::node_id lower_match(node& n, ast& ast, core_ast::ast& new_ast)
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
			auto test = lower(ast.get_node(child_children[0]), ast, new_ast);
			new_ast.link_child_parent(test, branch);

			auto body = lower(ast.get_node(child_children[1]), ast, new_ast);
			new_ast.link_child_parent(body, branch);
		}

		return branch;
	}

	core_ast::node_id lower_id(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(!has_children(n, ast));
		auto& data = ast.get_data<identifier>(n.data_index);

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
		id_node.size = (*ast
			.get_type_scope(n.type_scope_id)
			.resolve_variable(ast.get_data<identifier>(n.data_index), ast.type_scope_cb()))
			.type.calculate_size();
		return id;
	}

	core_ast::node_id lower_string(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		auto& str_data = ast.get_data<string>(n.data_index);

		auto str = new_ast.create_node(core_ast::node_type::STRING);
		auto& str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		return str;
	}

	core_ast::node_id lower_boolean(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));
		auto& bool_data = ast.get_data<boolean>(n.data_index);

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN);
		auto& bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return bool_id;
	}

	core_ast::node_id lower_number(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		auto& num_data = ast.get_data<number>(n.data_index);

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER);
		auto& num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		return num_id;
	}

	core_ast::node_id lower_function_call(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);

		auto id_id = lower(ast.get_node(children[0]), ast, new_ast);
		new_ast.link_child_parent(id_id, fun_id);

		auto param_id = lower(ast.get_node(children[1]), ast, new_ast);
		new_ast.link_child_parent(param_id, fun_id);

		new_ast.get_node(fun_id).size = (*ast
			.get_type_scope(ast.get_node(children[0]).type_scope_id)
			.resolve_variable(ast.get_data<identifier>(ast.get_node(children[0]).data_index), ast.type_scope_cb()))
			.type.calculate_size();

		return fun_id;
	}

	core_ast::node_id lower_module_declaration(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(ast.children_of(n).size() == 1);
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
		auto& children = ast.children_of(n);
		assert(children.size() == 3);

		auto dec = new_ast.create_node(core_ast::node_type::SET);

		auto& lhs_node = ast.get_node(children[0]);
		core_ast::node_id lhs;
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			lhs = lower(lhs_node, ast, new_ast);
			new_ast.link_child_parent(lhs, dec);

			new_ast.get_node(lhs).size = (*ast
				.get_type_scope(lhs_node.type_scope_id)
				.resolve_variable(ast.get_data<identifier>(lhs_node.data_index), ast.type_scope_cb()))
				.type.calculate_size();
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			lhs = new_ast.create_node(core_ast::node_type::IDENTIFIER_TUPLE);

			auto& children = ast.children_of(lhs_node);
			for (auto child : children)
			{
				auto new_child = lower(ast.get_node(child), ast, new_ast);
				new_ast.link_child_parent(new_child, lhs);
			}

			new_ast.link_child_parent(lhs, dec);
		}

		auto rhs = lower(ast.get_node(children[2]), ast, new_ast);
		new_ast.get_node(rhs).size = new_ast.get_node(lhs).size;
		new_ast.link_child_parent(rhs, dec);

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
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto set = new_ast.create_node(core_ast::node_type::SET);

		auto lhs = lower(ast.get_node(children[0]), ast, new_ast);
		auto& lhs_node = new_ast.get_node(lhs);
		assert(lhs_node.kind == core_ast::node_type::IDENTIFIER);
		new_ast.link_child_parent(lhs, set);

		auto fn_rhs = new_ast.create_node(core_ast::node_type::FUNCTION);
		new_ast.link_child_parent(fn_rhs, set);

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
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		auto ref = new_ast.create_node(core_ast::node_type::REFERENCE);
		auto child = lower(ast.get_node(children[0]), ast, new_ast);
		new_ast.link_child_parent(child, ref);
		return ref;
	}

	core_ast::node_id lower_array_value(node& n, ast& ast, core_ast::ast& new_ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.data_index == no_data);
		auto arr = new_ast.create_node(core_ast::node_type::TUPLE);

		auto& children = ast.children_of(n);
		for (auto child : children)
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
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		assert(n.data_index != no_data);

		auto lhs_node = lower(ast.get_node(children[0]), ast, new_ast);
		auto rhs_node = lower(ast.get_node(children[1]), ast, new_ast);

		core_ast::node_type new_node_type;
		switch (n.kind)
		{
		case node_type::ADDITION:       new_node_type = core_ast::node_type::ADD; break;
		case node_type::SUBTRACTION:    new_node_type = core_ast::node_type::SUB; break;
		case node_type::MULTIPLICATION: new_node_type = core_ast::node_type::MUL; break;
		case node_type::DIVISION:       new_node_type = core_ast::node_type::DIV; break;
		case node_type::MODULO:         new_node_type = core_ast::node_type::MOD; break;
		case node_type::EQUALITY:       new_node_type = core_ast::node_type::EQ;  break;
		case node_type::GREATER_OR_EQ:  new_node_type = core_ast::node_type::GEQ; break;
		case node_type::GREATER_THAN:   new_node_type = core_ast::node_type::GT;  break;
		case node_type::LESS_OR_EQ:     new_node_type = core_ast::node_type::LEQ; break;
		case node_type::LESS_THAN:      new_node_type = core_ast::node_type::LT;  break;
		case node_type::AND:            new_node_type = core_ast::node_type::AND; break;
		case node_type::OR:             new_node_type = core_ast::node_type::OR;  break;
		default: throw lower_error{ "Unknown binary op" };
		}

		auto new_node = new_ast.create_node(new_node_type);
		new_ast.link_child_parent(lhs_node, new_node);
		new_ast.link_child_parent(rhs_node, new_node);

		return new_node;
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
		auto& children = ast.children_of(root);

		core_ast::ast new_ast(core_ast::node_type::BLOCK);
		auto block = new_ast.root_id();

		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast);
			new_ast.link_child_parent(new_child, block);
		}

		return new_ast;
	}
}
#pragma once
#include "fe/data/ext_ast_data.h"
#include "fe/data/ext_ast.h"
#include "fe/data/type_scope.h"
#include "fe/pipeline/error.h"
#include "fe/pipeline/typechecker_stage.h"

namespace fe::ext_ast
{	
	// Helpers
	static void copy_parent_scope(node& n, ast& ast)
	{
		assert(n.parent_id);
		auto& parent = ast.get_node(*n.parent_id);
		assert(parent.type_scope_id);
		n.type_scope_id = *parent.type_scope_id;
	}

	// Typeof
	types::unique_type typeof_identifier(node& n, ast& ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(n.children.size() == 0);
		copy_parent_scope(n, ast);

		assert(n.type_scope_id);
		auto& scope = ast.get_type_scope(*n.type_scope_id);
		assert(n.data_index);
		auto& id = ast.get_data<identifier>(*n.data_index);
		auto& res = scope.resolve_variable(id);
		assert(res);
		return types::unique_type(res->type.copy());
	}

	types::unique_type typeof_number(node& n, ast& ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(n.children.size() == 0);
		copy_parent_scope(n, ast);

		assert(n.data_index);
		auto& number_data = ast.get_data<number>(*n.data_index);

		// #todo return the smallest int type that can fit the value
		return types::unique_type(new types::i64());
	}

	types::unique_type typeof_string(node& n, ast& ast)
	{
		assert(n.kind == node_type::STRING);
		assert(n.children.size() == 0);
		copy_parent_scope(n, ast);

		assert(n.data_index);
		auto& string_data = ast.get_data<string>(*n.data_index);

		return types::unique_type(new types::str());
	}

	types::unique_type typeof_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		assert(n.type_scope_id);
		if (n.parent_id)
			copy_parent_scope(n, ast);
		else
			n.name_scope_id = ast.create_name_scope();

		types::product_type* pt = new types::product_type();
		auto res = types::unique_type(pt);
		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			pt->product.push_back(typeof(child_node, ast));
		}
		return res;
	}

	types::unique_type typeof_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		auto res = typeof(id_node, ast);
		auto function_type = dynamic_cast<types::function_type*>(res.get());
		return std::move(function_type->to);
	}

	types::unique_type typeof_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
			n.type_scope_id = ast.create_type_scope(*ast.get_node(*n.parent_id).type_scope_id);
		if (n.children.size() == 0) return types::unique_type(new types::voidt());

		// Typecheck all children except for the last one, which might be a return value
		for (auto i = 0; i < n.children.size() - 1; i++)
		{
			auto& elem_node = ast.get_node(i);
			typecheck(elem_node, ast);
		}

		auto& last_node = ast.get_node(*n.children.rbegin());
		if (last_node.kind == node_type::BLOCK_RESULT)
		{
			// Block with return value
			return typeof(last_node, ast);
		}
		else
		{
			// Block with only statements
			typecheck(last_node, ast);
			return types::unique_type(new types::voidt());
		}
	}

	types::unique_type typeof_block_result(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);
		auto& child = ast.get_node(n.children[0]);
		return typeof(child, ast);
	}

	types::unique_type typeof_type_atom(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);

		auto& scope = ast.get_type_scope(*n.type_scope_id);
		auto res = scope.resolve_type(id_data);
		assert(res);
		return types::unique_type(res->type.copy());
	}

	types::unique_type typeof_binary_op(node& n, ast& ast)
	{
		assert(ext_ast::is_binary_op(n.kind));
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(n.children[0]);
		auto& rhs_node = ast.get_node(n.children[1]);

		auto lhs_type = typeof(lhs_node, ast);
		auto rhs_type = typeof(rhs_node, ast);

		switch (n.kind)
		{
		case node_type::ADDITION:
		case node_type::SUBTRACTION:
		case node_type::MULTIPLICATION:
		case node_type::DIVISION:
		case node_type::MODULO:
		{
			assert(*lhs_type == &types::i64());
			assert(*rhs_type == &types::i64());

			return types::unique_type(new types::i64());
		}
		case node_type::EQUALITY:
		case node_type::GREATER_OR_EQ:
		case node_type::GREATER_THAN:
		case node_type::LESS_OR_EQ:
		case node_type::LESS_THAN:
		{
			assert(*lhs_type == &types::i64());
			assert(*rhs_type == &types::i64());

			return types::unique_type(new types::boolean());
		}
		default:
			throw std::runtime_error("Typeof for binary op not implemented");
		}
	}

	types::unique_type typeof_atom_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_DECLARATION);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& type_atom_child = ast.get_node(n.children[0]);
		assert(type_atom_child.kind == node_type::TYPE_ATOM);
		return typeof(type_atom_child, ast);
	}

	types::unique_type typeof_tuple_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE_DECLARATION);
		copy_parent_scope(n, ast);

		std::vector<types::unique_type> types;
		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			assert(child_node.kind == node_type::ATOM_DECLARATION || child_node.kind == node_type::TUPLE_DECLARATION);
			types.push_back(typeof(child_node, ast));
		}

		return types::unique_type(new types::product_type(std::move(types)));
	}

	// Typecheck

	void typecheck_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		auto res = typeof(id_node, ast);
		auto function_type = dynamic_cast<types::function_type*>(res.get());
		auto& from_type = *function_type->from;

		auto& param_node = ast.get_node(n.children[1]);
		auto param_type = typeof(param_node, ast);
		// #todo replace with throws
		assert(from_type == &*param_type);
	}

	void typecheck_match_branch(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH_BRANCH);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& test_node = ast.get_node(n.children[0]);
		auto test_type = typeof(test_node, ast);

		auto& code_node = ast.get_node(n.children[1]);
		auto code_type = typeof(code_node, ast);

		// #todo check test_node is of type bool and set this type to code_node type
	}

	void typecheck_match(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH);
		copy_parent_scope(n, ast);

		for (auto branch : n.children)
		{
			auto& branch_node = ast.get_node(branch);
			typecheck(branch_node, ast);

			// #todo check element is same as others
		}

		// #todo set this type to others
	}

	void typecheck_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
			n.type_scope_id = ast.create_type_scope(*ast.get_node(*n.parent_id).type_scope_id);

		for (auto element : n.children)
		{
			auto& elem_node = ast.get_node(element);
			typecheck(elem_node, ast);
		}

		if (n.children.size() > 0)
		{
			// #todo set this type to the last child type if it is an expression, otherwise void
		}
	}

	void declare_atom_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_DECLARATION);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& type_node = ast.get_node(n.children[0]);
		auto atom_type = typeof(type_node, ast);

		auto& id_node = ast.get_node(n.children[1]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);

		auto& scope = ast.get_type_scope(*n.type_scope_id);
		scope.set_type(id_data.segments[0], std::move(atom_type));
	}

	void declare_tuple_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE_DECLARATION);
		copy_parent_scope(n, ast);

		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			declare_atom_declaration(child_node, ast);
		}
	}

	void typecheck_function(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION);
		assert(n.children.size() == 4); // name from to body
		auto& parent_scope_id = *ast.get_node(*n.parent_id).type_scope_id;
		n.type_scope_id = ast.create_type_scope(parent_scope_id);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_node_data = ast.get_data<identifier>(*id_node.data_index);

		auto& from_node = ast.get_node(n.children[1]);
		if (from_node.kind == node_type::ATOM_DECLARATION)
			declare_atom_declaration(from_node, ast);
		auto from_type = typeof(from_node, ast);
		auto& to_node = ast.get_node(n.children[2]);
		auto to_type = typeof(to_node, ast);

		// Put function name in parent scope
		auto& parent_scope = ast.get_type_scope(parent_scope_id);
		parent_scope.set_type(id_node_data.segments[0], types::unique_type(new types::function_type(
			types::unique_type(from_type->copy()), types::unique_type(to_type->copy()))));

		// #todo test
		auto& body_node = ast.get_node(n.children[3]);
		auto body_type = typeof(body_node, ast);

		// #todo change to throws
		assert(*body_type == &*to_type);
	}

	void typecheck_type_definition(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);
		assert(id_data.segments.size() == 1);

		auto& type_node = ast.get_node(n.children[1]);
		auto rhs = typeof(type_node, ast);

		auto& scope = ast.get_type_scope(*n.type_scope_id);
		scope.define_type(id_data.segments[0], types::unique_type(rhs->copy()));

		auto function_type = types::unique_type(new types::function_type(
			types::unique_type(rhs->copy()),
			types::unique_type(rhs->copy())
		));
		scope.set_type(id_data.segments[0], std::move(function_type));
	}

	std::optional<types::unique_type> resolve_offsets(types::type& t, ast& ast, std::vector<size_t> offsets)
	{
		if (offsets.size() == 0) return types::unique_type(t.copy());
		if (auto pt = dynamic_cast<types::product_type*>(&t))
		{
			if (offsets.size() == 1)
			{
				return types::unique_type(pt->product.at(offsets[0])->copy());
			}
			else
			{
				std::vector<size_t> new_offsets(offsets.begin() + 1, offsets.end());
				return resolve_offsets(*pt->product.at(offsets[0]), ast, new_offsets);
			}
		}
		else
		{
			assert(!"Unimplemented");
		}

	
		return std::nullopt;
	}

	void typecheck_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::DECLARATION);
		// id type value
		assert(n.children.size() == 3);
		copy_parent_scope(n, ast);

		auto& scope = ast.get_type_scope(*n.name_scope_id);

		auto& type_node = ast.get_node(n.children[1]);
		assert(type_node.data_index);
		auto& type_id_data = ast.get_data<identifier>(*type_node.data_index);
		auto type_lookup = scope.resolve_type(type_id_data);
		auto& type = type_lookup->type;

		auto& val_node = ast.get_node(n.children[2]);
		auto val_type = typeof(val_node, ast);
		assert(type == &*val_type);

		auto& lhs_node = ast.get_node(n.children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			assert(lhs_node.data_index);
			auto& id_data = ast.get_data<identifier>(*lhs_node.data_index);
			assert(id_data.segments.size() == 1);
			scope.set_type(id_data.segments[0], std::move(val_type));
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{

		}
		else
		{
			assert(!"Illegal lhs of declaration");
		}
	}

	void typecheck_assignment(node& n, ast& ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		// id value
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& scope = ast.get_type_scope(*n.type_scope_id);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);
		auto res = scope.resolve_variable(id_data);
		assert(res);
		auto& id_type = res->type;

		auto& value_node = ast.get_node(n.children[1]);
		auto value_type = typeof(value_node, ast);

		assert(id_type == &*value_type);
	}

	// Type expressions
	// #todo change to typeof since the value of these nodes when interpreted is not actually the types calculated

	void typecheck_type_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_TUPLE);

		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			typecheck(child_node, ast);
		}

		// #todo set this type to product of child types
	}

	void typecheck_function_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_TYPE);
		assert(n.children.size() == 2);

		auto& from_node = ast.get_node(n.children[0]);
		typecheck(from_node, ast);
		auto& to_node = ast.get_node(n.children[1]);
		typecheck(to_node, ast);

		// #todo set this type to function_type(from, to)
	}

	void typecheck_reference_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::REFERENCE_TYPE);
		assert(n.children.size() == 1);

		auto& child_node = ast.get_node(n.children[0]);
		typecheck(child_node, ast);
		// #todo set this type
	}

	void typecheck_array_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_TYPE);
		assert(n.children.size() == 1);

		auto& child_node = ast.get_node(n.children[0]);
		typecheck(child_node, ast);
		// #todo set this type
	}

	// End type expressions

	void typecheck_reference(node& n, ast& ast)
	{
		assert(n.kind == node_type::REFERENCE);
		assert(n.children.size() == 1);

		auto& child_node = ast.get_node(n.children[0]);
		typecheck(child_node, ast);
		// #todo set this type to reference type
	}

	void typecheck_array_value(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		if (n.children.size() > 0)
		{
			for (auto child : n.children)
			{
				auto& child_node = ast.get_node(child);
				typecheck(child_node, ast);
				// #todo check all types are same
			}
		}
		else
		{
			// #todo set type to void
		}
	}

	void typecheck_while_loop(node& n, ast& ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		// test body
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& test_node = ast.get_node(n.children[0]);
		auto type = typeof(test_node, ast);

		assert(*type == &types::boolean());

		auto& body_node = ast.get_node(n.children[1]);
		typecheck(body_node, ast);
	}

	void typecheck_if_statement(node& n, ast& ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		// test body
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& test_node = ast.get_node(n.children[0]);
		auto& test_node_type = typeof(test_node, ast);

		auto& body_node = ast.get_node(n.children[1]);
		auto& body_node_type = typeof(body_node, ast);

		assert(*test_node_type == &types::boolean());
		// #todo check test type is boolean and set this type to body type
	}

	void typecheck_number(node& n, ast& ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(n.children.size() == 0);
	}

	void typecheck(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::FUNCTION_CALL:     typecheck_function_call(n, ast);     break;
		case node_type::MATCH_BRANCH:      typecheck_match_branch(n, ast);      break;
		case node_type::MATCH:             typecheck_match(n, ast);             break;
		case node_type::BLOCK:             typecheck_block(n, ast);             break;
		case node_type::FUNCTION:          typecheck_function(n, ast);          break;
		case node_type::TYPE_DEFINITION:   typecheck_type_definition(n, ast);   break;
		case node_type::DECLARATION:       typecheck_declaration(n, ast);       break;
		case node_type::ASSIGNMENT:        typecheck_assignment(n, ast);        break;
		case node_type::TYPE_TUPLE:        typecheck_type_tuple(n, ast);        break;
		case node_type::FUNCTION_TYPE:     typecheck_function_type(n, ast);     break;
		case node_type::REFERENCE_TYPE:    typecheck_reference_type(n, ast);    break;
		case node_type::ARRAY_TYPE:        typecheck_array_type(n, ast);        break;
		case node_type::REFERENCE:         typecheck_reference(n, ast);         break;
		case node_type::ARRAY_VALUE:       typecheck_array_value(n, ast);       break;
		case node_type::WHILE_LOOP:        typecheck_while_loop(n, ast);        break;
		case node_type::IF_STATEMENT:      typecheck_if_statement(n, ast);      break;
		case node_type::NUMBER:            typecheck_number(n, ast);            break;
		case node_type::MODULE_DECLARATION:
		case node_type::IMPORT_DECLARATION: break;
		default: assert(!"This node cannot be typechecked");
		}
	}

	types::unique_type typeof(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::IDENTIFIER:        return typeof_identifier(n, ast);
		case node_type::NUMBER:            return typeof_number(n, ast);
		case node_type::STRING:            return typeof_string(n, ast);
		case node_type::TUPLE:             return typeof_tuple(n, ast);
		case node_type::FUNCTION_CALL:     return typeof_function_call(n, ast);
		case node_type::BLOCK:             return typeof_block(n, ast);
		case node_type::BLOCK_RESULT:      return typeof_block_result(n, ast);
		case node_type::TYPE_ATOM:         return typeof_type_atom(n, ast);
		case node_type::ATOM_DECLARATION:  return typeof_atom_declaration(n, ast);
		case node_type::TUPLE_DECLARATION: return typeof_tuple_declaration(n, ast);
		default:
			if (ext_ast::is_binary_op(n.kind)) return typeof_binary_op(n, ast);

			assert(!"This node cannot be typeofed");
			throw std::runtime_error("Node cannot be typeofed");
		}
	}
}
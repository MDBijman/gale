#include "fe/data/extended_ast.h"

// Resolving
namespace fe::ext_ast
{
	// Helpers
	void copy_parent_scope(node& n, ast& ast)
	{
		assert(n.parent_id.has_value());
		auto& parent = ast.get_node(n.parent_id.value());
		assert(parent.scope_index.has_value());
		n.scope_index = parent.scope_index.value();
	}

	// Node resolvers
	void resolve_assignment(node& n, ast& ast)
	{
		assert(n.type == node_type::ASSIGNMENT);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_tuple(node& n, ast& ast)
	{
		assert(n.type == node_type::TUPLE);
		copy_parent_scope(n, ast);

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_block(node& n, ast& ast)
	{
		assert(n.type == node_type::BLOCK);
		n.scope_index = ast.create_scope();

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_function(node& n, ast& ast)
	{
		/*
		* A function has 4 children: identifier, from signature, to signature, and body
		* We add the identifier to the parent scope, so that the function can be resolvable
		* We also directly define the identifier, so that the function can recurse
		* We then 
		*  resolve the from signature, which adds the paramaters to the scope
		*  resolve the to signature, to check that the return type is well defined
		*  resolve the body
		*/

		assert(n.type == node_type::FUNCTION);
		assert(n.parent_id.has_value());
		assert(n.children.size() == 4);

		auto& parent = ast.get_node(n.parent_id.value());

		assert(parent.scope_index.has_value());
		auto& parent_scope = ast.get_scope(parent.scope_index.value());

		auto& id_node = ast.get_node(n.children.at(0));
		assert(id_node.type == node_type::IDENTIFIER);
		assert(id_node.data_index.has_value());

		auto& id_data = ast.get_data<identifier>(id_node.data_index.value());
		assert(id_data.segments.size() == 1);

		parent_scope.declare_variable(id_data.segments[0], identifier{ {"__function"} });

		n.scope_index = ast.create_scope();

		resolve(ast.get_node(n.children[1]), ast);
		resolve(ast.get_node(n.children[2]), ast);
		resolve(ast.get_node(n.children[3]), ast);
	}

	void resolve_while_loop(node& n, ast& ast)
	{
		assert(n.type == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_if_statement(node& n, ast& ast)
	{
		assert(n.type == node_type::IF_STATEMENT);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_match_branch(node& n, ast& ast)
	{
		assert(n.type == node_type::MATCH_BRANCH);
		assert(n.children.size() == 2);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_match(node& n, ast& ast)
	{
		assert(n.type == node_type::MATCH);
		// At least an expression and a single branch
		assert(n.children.size() >= 2);

		copy_parent_scope(n, ast);

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_id(node& n, ast& ast)
	{
		assert(n.type == node_type::IDENTIFIER);
		copy_parent_scope(n, ast);

		assert(n.data_index.has_value());
		auto& id_data = ast.get_data<identifier>(n.data_index.value());
		ast.get_scope(n.scope_index.value()).resolve_variable(id_data);
	}

	void resolve_literal(node& n, ast& ast)
	{
		assert(n.type == node_type::LITERAL);
		copy_parent_scope(n, ast);
	}

	void resolve_function_call(node& n, ast& ast)
	{
		assert(n.type == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	// Helper for declarations
	void declare_lhs(node& lhs, node& type, ast& ast)
	{
		assert(lhs.type == node_type::IDENTIFIER || lhs.type == node_type::IDENTIFIER_TUPLE);
		assert(type.type == node_type::IDENTIFIER);

		if (lhs.type == node_type::IDENTIFIER)
		{
			assert(lhs.data_index.has_value());
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index.value());
			assert(type.data_index.has_value());
			auto& type_id = ast.get_data<identifier>(type.data_index.value());

			ast.get_scope(lhs.scope_index.value()).declare_variable(lhs_id.segments[0], type_id);
		}
		else if (lhs.type == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"Not implemented");
		}
	}

	void resolve_declaration(node& n, ast& ast)
	{
		assert(n.type == node_type::DECLARATION);
		// identifier type_name value
		assert(n.children.size() == 3);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(n.children[0]);
		lhs_node.scope_index = n.scope_index;
		auto& type_node = ast.get_node(n.children[1]);
		type_node.scope_index = n.scope_index;
		declare_lhs(lhs_node, type_node, ast);

		auto& rhs_node = ast.get_node(n.children[2]);
		resolve(rhs_node, ast);
	}

	void resolve_type_definition(node& n, ast& ast)
	{
		assert(n.type == node_type::TYPE_DEFINITION);
		// identifier type_dec
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.type == node_type::IDENTIFIER);

		auto& type_node = ast.get_node(n.children[1]);
		assert(type_node.type == node_type::DECLARATION);
		resolve(id_node, ast);
	}

	void resolve_reference(node& n, ast& ast)
	{
		assert(n.type == node_type::REFERENCE);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& child_node = ast.get_node(n.children[0]);
		resolve(child_node, ast);
	}

	void resolve_array_value(node& n, ast& ast)
	{
		assert(n.type == node_type::ARRAY_VALUE);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& child_node = ast.get_node(n.children[0]);
		resolve(child_node, ast);
	}

	void resolve(node& n, ast& ast)
	{
		switch (n.type)
		{
		case node_type::ASSIGNMENT:        resolve_assignment(n, ast);      break;
		case node_type::TUPLE:             resolve_tuple(n, ast);           break;
		case node_type::BLOCK:             resolve_block(n, ast);           break;
		case node_type::FUNCTION:          resolve_function(n, ast);        break;
		case node_type::WHILE_LOOP:        resolve_while_loop(n, ast);      break;
		case node_type::IF_STATEMENT:      resolve_if_statement(n, ast);    break;
		case node_type::MATCH_BRANCH:      resolve_match_branch(n, ast);    break;
		case node_type::MATCH:             resolve_match(n, ast);           break;
		case node_type::IDENTIFIER:        resolve_id(n, ast);              break;
		case node_type::LITERAL:           resolve_literal(n, ast);         break;
		case node_type::FUNCTION_CALL:     resolve_function_call(n, ast);   break;
		case node_type::DECLARATION:       resolve_declaration(n, ast);     break;
		case node_type::TYPE_DEFINITION:   resolve_type_definition(n, ast); break;
		case node_type::REFERENCE:         resolve_reference(n, ast);       break;
		case node_type::ARRAY_VALUE:       resolve_array_value(n, ast);     break;
		default: assert(!"Node type not resolvable");
		}
	}
}

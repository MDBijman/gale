#include "fe/data/ext_ast.h"
#include "fe/pipeline/resolution_stage.h"

// Resolving
namespace fe::ext_ast
{
	// Helpers
	static void copy_parent_scope(node& n, ast& ast)
	{
		assert(n.parent_id);
		auto& parent = ast.get_node(n.parent_id.value());
		assert(parent.name_scope_id);
		n.name_scope_id = parent.name_scope_id.value();
	}

	// Node resolvers
	void resolve_assignment(node& n, ast& ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(n.children[0]);
		assert(lhs_node.data_index);
		auto& lhs_data = ast.get_data<identifier>(*lhs_node.data_index);
		auto& scope = ast.get_name_scope(*n.name_scope_id);
		auto res = scope.resolve_variable(lhs_data);
		assert(res);
		lhs_data.scope_distance = res->scope_distance;

		auto& value_node = ast.get_node(n.children[1]);
		resolve(value_node, ast);
	}

	void resolve_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		auto& parent = ast.get_node(*n.parent_id);
		assert(parent.name_scope_id);
		n.name_scope_id = ast.create_name_scope(*parent.name_scope_id);

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
		{
			auto& parent = ast.get_node(*n.parent_id);
			assert(parent.name_scope_id);
			n.name_scope_id = ast.create_name_scope(*parent.name_scope_id);
		}

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_block_result(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);
		auto& child = ast.get_node(n.children[0]);
		resolve(child, ast);
	}

	// #todo make this regular resolve?
	void declare_type_atom(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(id_node.data_index.value());

		assert(n.name_scope_id);
		auto& scope = ast.get_name_scope(*n.name_scope_id);
		auto res = scope.resolve_type(id_data);
		assert(res);
		id_data.scope_distance = res->scope_distance;
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

		assert(n.kind == node_type::FUNCTION);
		copy_parent_scope(n, ast);
		assert(n.children.size() == 4);

		auto& parent = ast.get_node(n.parent_id.value());

		assert(parent.name_scope_id);
		auto& parent_scope = ast.get_name_scope(parent.name_scope_id.value());

		auto& id_node = ast.get_node(n.children.at(0));
		assert(id_node.kind == node_type::IDENTIFIER);
		assert(id_node.data_index);

		auto& id_data = ast.get_data<identifier>(id_node.data_index.value());
		assert(id_data.segments.size() == 1);

		parent_scope.declare_variable(id_data.segments[0]);
		parent_scope.define_variable(id_data.segments[0]);

		n.name_scope_id = ast.create_name_scope(*parent.name_scope_id);

		// #todo fix for type tuple
		resolve(ast.get_node(n.children[1]), ast);
		declare_type_atom(ast.get_node(n.children[2]), ast);
		resolve(ast.get_node(n.children[3]), ast);
	}

	void resolve_while_loop(node& n, ast& ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_if_statement(node& n, ast& ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_match_branch(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH_BRANCH);
		assert(n.children.size() == 2);
		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_match(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH);
		// At least an expression and a single branch
		assert(n.children.size() >= 2);

		copy_parent_scope(n, ast);

		for (auto child : n.children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_id(node& n, ast& ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		copy_parent_scope(n, ast);
		auto& scope = ast.get_name_scope(*n.name_scope_id);

		assert(n.data_index);
		auto& id_data = ast.get_data<identifier>(*n.data_index);
		auto resolved = scope.resolve_variable(id_data);
		assert(resolved);
		id_data.scope_distance = resolved->scope_distance;
	}

	void resolve_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(n.children[0]), ast);
		resolve(ast.get_node(n.children[1]), ast);
	}

	void resolve_type_definition(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		// identifier type_dec
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.kind == node_type::IDENTIFIER);

		auto& type_node = ast.get_node(n.children[1]);
		assert(type_node.kind == node_type::DECLARATION);
		resolve(id_node, ast);
	}

	// Helper for declarations
	void declare_lhs(node& lhs, ast& ast)
	{
		assert(lhs.kind == node_type::IDENTIFIER || lhs.kind == node_type::IDENTIFIER_TUPLE);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index.value());
			ast.get_name_scope(lhs.name_scope_id.value()).declare_variable(lhs_id.segments[0]);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"Not implemented");
		}
	}

	void define_lhs(node& lhs, ast& ast)
	{
		assert(lhs.kind == node_type::IDENTIFIER || lhs.kind == node_type::IDENTIFIER_TUPLE);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index.value());
			ast.get_name_scope(lhs.name_scope_id.value()).define_variable(lhs_id.segments[0]);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"Not implemented");
		}
	}

	void resolve_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::DECLARATION);
		// identifier type_name value
		assert(n.children.size() == 3);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(n.children[0]);
		lhs_node.name_scope_id = n.name_scope_id;
		auto& type_node = ast.get_node(n.children[1]);
		type_node.name_scope_id = n.name_scope_id;
		declare_lhs(lhs_node, ast);

		auto& rhs_node = ast.get_node(n.children[2]);
		resolve(rhs_node, ast);

		define_lhs(lhs_node, ast);
	}

	void resolve_atom_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_DECLARATION);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[1]);
		assert(id_node.kind == node_type::IDENTIFIER);
		assert(id_node.data_index);
		auto& node_id = ast.get_data<identifier>(id_node.data_index.value());
		ast.get_name_scope(n.name_scope_id.value()).declare_variable(node_id.segments[0]);
	}

	void resolve_reference(node& n, ast& ast)
	{
		assert(n.kind == node_type::REFERENCE);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& child_node = ast.get_node(n.children[0]);
		resolve(child_node, ast);
	}

	void resolve_array_value(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.children.size() == 1);
		copy_parent_scope(n, ast);

		auto& child_node = ast.get_node(n.children[0]);
		resolve(child_node, ast);
	}

	void resolve_binary_op(node& n, ast& ast)
	{
		assert(ext_ast::is_binary_op(n.kind));
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(n.children[0]);
		resolve(lhs_node, ast);
		auto& rhs_node = ast.get_node(n.children[1]);
		resolve(rhs_node, ast);
	}

	void resolve(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:        resolve_assignment(n, ast);       break;
		case node_type::TUPLE:             resolve_tuple(n, ast);            break;
		case node_type::BLOCK:             resolve_block(n, ast);            break;
		case node_type::BLOCK_RESULT:      resolve_block_result(n, ast);     break;
		case node_type::FUNCTION:          resolve_function(n, ast);         break;
		case node_type::WHILE_LOOP:        resolve_while_loop(n, ast);       break;
		case node_type::IF_STATEMENT:      resolve_if_statement(n, ast);     break;
		case node_type::MATCH_BRANCH:      resolve_match_branch(n, ast);     break;
		case node_type::MATCH:             resolve_match(n, ast);            break;
		case node_type::IDENTIFIER:        resolve_id(n, ast);               break;
		case node_type::FUNCTION_CALL:     resolve_function_call(n, ast);    break;
		case node_type::TYPE_DEFINITION:   resolve_type_definition(n, ast);  break;
		case node_type::DECLARATION:       resolve_declaration(n, ast);      break;
		case node_type::ATOM_DECLARATION:  resolve_atom_declaration(n, ast); break;
		case node_type::REFERENCE:         resolve_reference(n, ast);        break;
		case node_type::ARRAY_VALUE:       resolve_array_value(n, ast);      break;
		case node_type::STRING:            copy_parent_scope(n, ast);        break;
		case node_type::BOOLEAN:           copy_parent_scope(n, ast);        break;
		case node_type::NUMBER:            copy_parent_scope(n, ast);        break;
		case node_type::MODULE_DECLARATION: break;
		case node_type::IMPORT_DECLARATION: break;
		default:
			if (ext_ast::is_binary_op(n.kind)) { resolve_binary_op(n, ast); return; }

			assert(!"Node type not resolvable");
			throw std::runtime_error("Node type not resolvable");
		}
	}
}

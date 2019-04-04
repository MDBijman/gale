#include "fe/pipeline/resolution_stage.h"
#include "fe/data/ext_ast.h"
#include "fe/pipeline/error.h"

// Resolving
namespace fe::ext_ast
{
	// Helpers
	static void copy_parent_scope(node &n, ast &ast)
	{
		auto &parent = ast[n.parent_id];
		assert(parent.name_scope_id != no_scope);
		n.name_scope_id = parent.name_scope_id;
	}

	void resolve(node &n, ast &ast);

	// Node resolvers
	void resolve_assignment(node &n, ast &ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);
		auto &scope = ast.get_name_scope(n.name_scope_id);

		auto &lhs_node = ast[children[0]];
		copy_parent_scope(lhs_node, ast);
		auto &lhs_data = ast.get_data<identifier>(lhs_node.data_index);
		assert(lhs_data.module_path.size() == 0);

		auto res = scope.resolve_variable(lhs_data.name, ast.name_scope_cb());
		assert(res);
		lhs_data.scope_distance = res->scope_distance;

		auto &value_node = ast[children[1]];
		resolve(value_node, ast);
	}

	void resolve_tuple(node &n, ast &ast)
	{
		assert(n.kind == node_type::TUPLE);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);
		for (auto child : children) resolve(ast[child], ast);
	}

	void resolve_block(node &n, ast &ast)
	{
		assert(n.kind == node_type::BLOCK);
		auto &children = ast.children_of(n);
		if (n.parent_id)
		{
			auto &parent = ast[n.parent_id];
			n.name_scope_id = ast.create_name_scope(parent.name_scope_id);
		}
		// If we dont have a parent we already have a name scope.
		else
			assert(n.name_scope_id != no_scope);

		for (auto child : children) resolve(ast[child], ast);
	}

	void resolve_block_result(node &n, ast &ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);
		auto &child = ast[children[0]];
		resolve(child, ast);
	}

	// #todo make this regular resolve?
	void declare_type_atom(node &n, ast &ast)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto &id_node = ast[children[0]];
		auto &id_data = ast.get_data<identifier>(id_node.data_index);
		assert(id_data.module_path.size() == 0);
		auto &type_name = id_data.name;

		auto &scope = ast.get_name_scope(n.name_scope_id);
		auto res = scope.resolve_type(type_name, ast.name_scope_cb());
		assert(res);
		id_data.scope_distance = res->scope_distance;
	}

	void declare_assignable(node &assignable, ast &ast)
	{
		copy_parent_scope(assignable, ast);
		if (assignable.kind == node_type::IDENTIFIER)
		{
			auto &data = ast.get_data<identifier>(assignable.data_index);
			ast.get_name_scope(assignable.name_scope_id)
			  .declare_variable(data.full, assignable.id);
			ast.get_name_scope(assignable.name_scope_id).define_variable(data.full);
		}
		else if (assignable.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto &children = ast.children_of(assignable);
			for (auto &child : children) { declare_assignable(ast[child], ast); }
		}
	}

	void resolve_lambda(node &n, ast &ast)
	{
		/*
		 * A lambda has 2 children: assignable and expression (body)
		 * We add the identifiers in the assignable to a new scope
		 * And resolve the body
		 */

		assert(n.kind == node_type::LAMBDA);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto &parent = ast[n.parent_id];
		n.name_scope_id = ast.create_name_scope(parent.name_scope_id);

		auto &assignable_node = ast[children[0]];
		assert(assignable_node.kind == node_type::IDENTIFIER ||
		       assignable_node.kind == node_type::IDENTIFIER_TUPLE);

		declare_assignable(assignable_node, ast);

		// Resolve body
		resolve(ast[children[1]], ast);
	}

	void resolve_while_loop(node &n, ast &ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast[children[0]], ast);
		resolve(ast[children[1]], ast);
	}

	void resolve_if_statement(node &n, ast &ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);

		// #cleanup create generic resolve_all_children
		for (auto child : children) resolve(ast[child], ast);
	}

	void resolve_pattern(node &n, ast &ast)
	{
		copy_parent_scope(n, ast);
		auto &scope = ast.get_name_scope(n.name_scope_id);

		switch (n.kind)
		{
		case node_type::FUNCTION_CALL:
		{
			auto &id = ast[ast.get_children(n.children_id)[0]];
			auto &name = ast.get_data<identifier>(id.data_index);
			name.scope_distance =
			  scope.resolve_type(name.name, ast.name_scope_cb())->scope_distance;

			auto &param = ast[ast.get_children(n.children_id)[1]];
			resolve_pattern(param, ast);
			break;
		}
		case node_type::IDENTIFIER:
		{
			auto &name = ast.get_data<identifier>(n.data_index).name;
			scope.declare_variable(name, n.id);
			scope.define_variable(name);
			break;
		}
		case node_type::TUPLE:
			for (auto &child : ast.get_children(n.children_id))
				resolve_pattern(ast[child], ast);
			break;
		case node_type::NUMBER:
		case node_type::BOOLEAN: break;
		default: assert(!"Invalid pattern"); break;
		}
	}

	void resolve_match_branch(node &n, ast &ast)
	{
		assert(n.kind == node_type::MATCH_BRANCH);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		n.name_scope_id = ast.create_name_scope(ast[n.parent_id].name_scope_id);

		resolve_pattern(ast[children[0]], ast);
		resolve(ast[children[1]], ast);
	}

	void resolve_match(node &n, ast &ast)
	{
		assert(n.kind == node_type::MATCH);
		auto &children = ast.children_of(n);
		// At least an expression and a single branch
		assert(children.size() >= 2);
		copy_parent_scope(n, ast);

		for (auto child : children) resolve(ast[child], ast);
	}

	void resolve_id(node &n, ast &ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		copy_parent_scope(n, ast);
		auto &scope = ast.get_name_scope(n.name_scope_id);

		auto &id = ast.get_data<identifier>(n.data_index);

		if (auto resolved_as_var =
		      scope.resolve_variable(id.module_path, id.name, ast.name_scope_cb());
		    resolved_as_var)
		{ id.scope_distance = resolved_as_var->scope_distance; }
		else if (auto resolved_as_type =
			   scope.resolve_type(id.module_path, id.name, ast.name_scope_cb());
			 resolved_as_type)
		{
			id.scope_distance = resolved_as_type->scope_distance;
		}
		else
		{
			throw resolution_error{ std::string("Unknown identifier: ").append(id) };
		}
	}

	void resolve_function_call(node &n, ast &ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast[children[0]], ast);
		resolve(ast[children[1]], ast);
	}

	void resolve_array_access(node &n, ast &ast)
	{
		assert(n.kind == node_type::ARRAY_ACCESS);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast[children[0]], ast);
		resolve(ast[children[1]], ast);
	}

	void resolve_type_definition(node &n, ast &ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto &children = ast.children_of(n);
		// identifier type_dec
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &id_node = ast[children[0]];
		assert(id_node.kind == node_type::IDENTIFIER);
		auto &id_data = ast.get_data<identifier>(id_node.data_index);
		assert(id_data.module_path.size() == 0);

		auto &scope = ast.get_name_scope(n.name_scope_id);

		auto &type_node = ast[children[1]];
		scope.define_type(id_data.name, type_node.id);
	}

	node_id product_type_element(node &type_node, ast &ast, size_t index)
	{
		copy_parent_scope(type_node, ast);
		auto &children = ast.children_of(type_node);
		auto &scope = ast.get_name_scope(type_node.name_scope_id);

		if (type_node.kind == node_type::IDENTIFIER)
		{
			auto &id = ast.get_data<identifier>(type_node.data_index);
			auto res = scope.resolve_type(id.module_path, id.name, ast.name_scope_cb());
			assert(res);
			auto &referenced_type_node = ast[res->declaration_node];
			return product_type_element(referenced_type_node, ast, index);
		}
		else if (type_node.kind == node_type::ATOM_TYPE)
		{
			assert(children.size() == 1);
			auto &id_node = ast[children[0]];
			return product_type_element(id_node, ast, index);
		}
		else if (type_node.kind == node_type::TUPLE_TYPE)
		{
			return children.at(index);
		}

		assert(!"Cannot get element of this type, not a product");
	}

	// Helper for declarations
	void declare_lhs(node &lhs, ast &ast, node &type_node)
	{
		assert(lhs.kind == node_type::IDENTIFIER ||
		       lhs.kind == node_type::IDENTIFIER_TUPLE);
		copy_parent_scope(lhs, ast);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			auto &lhs_id = ast.get_data<identifier>(lhs.data_index);

			lhs_id.type_node = type_node.id;
			auto &scope = ast.get_name_scope(lhs.name_scope_id);
			scope.declare_variable(lhs_id.name, lhs.id);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			size_t i = 0;
			for (auto child : ast.children_of(lhs))
			{
				auto new_type_id = product_type_element(type_node, ast, i);
				assert(new_type_id);
				auto &new_type_node = ast[new_type_id];
				auto &new_lhs = ast[child];
				declare_lhs(new_lhs, ast, new_type_node);
				i++;
			}
		}
	}

	void define_lhs(node &lhs, ast &ast)
	{
		assert(lhs.kind == node_type::IDENTIFIER ||
		       lhs.kind == node_type::IDENTIFIER_TUPLE);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto &lhs_id = ast.get_data<identifier>(lhs.data_index);
			assert(lhs_id.module_path.size() == 0);
			ast.get_name_scope(lhs.name_scope_id).define_variable(lhs_id.name);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			for (auto child : ast.children_of(lhs))
			{
				auto &new_lhs = ast[child];
				define_lhs(new_lhs, ast);
			}
		}
	}

	void resolve_declaration(node &n, ast &ast)
	{
		assert(n.kind == node_type::DECLARATION);
		// identifier type_name value
		auto &children = ast.children_of(n);
		assert(children.size() == 3);
		copy_parent_scope(n, ast);

		auto &lhs_node = ast[children[0]];
		lhs_node.name_scope_id = n.name_scope_id;
		auto &type_node = ast[children[1]];
		type_node.name_scope_id = n.name_scope_id;
		resolve(type_node, ast);
		declare_lhs(lhs_node, ast, type_node);

		auto &rhs_node = ast[children[2]];
		resolve(rhs_node, ast);

		define_lhs(lhs_node, ast);
	}

	void resolve_function(node &n, ast &ast)
	{
		/*
		 * A function has 3 children: id, type, and lambda
		 * Skip declaring the function since we do that in an earlier pass
		 */

		assert(n.kind == node_type::FUNCTION);
		auto &children = ast.children_of(n);
		assert(children.size() == 3);

		auto &lhs_node = ast[children[0]];
		auto &type_node = ast[children[1]];
		lhs_node.name_scope_id = n.name_scope_id;
		type_node.name_scope_id = n.name_scope_id;
		resolve(type_node, ast);
		resolve_lambda(ast[children[2]], ast);
	}

	void resolve_reference(node &n, ast &ast)
	{
		assert(n.kind == node_type::REFERENCE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto &child_node = ast[children[0]];
		resolve(child_node, ast);
	}

	void resolve_array_value(node &n, ast &ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		auto &children = ast.children_of(n);
		copy_parent_scope(n, ast);

		for (auto &child : children) resolve(ast[child], ast);
	}

	void resolve_type_atom(node &n, ast &ast)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);
		resolve(ast[children[0]], ast);
	}

	void resolve_type_tuple(node &n, ast &ast)
	{
		assert(n.kind == node_type::TUPLE_TYPE);
		auto &children = ast.children_of(n);
		copy_parent_scope(n, ast);
		for (auto &child : children) { resolve(ast[child], ast); }
	}

	void resolve_function_type(node &n, ast &ast)
	{
		assert(n.kind == node_type::FUNCTION_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast[children[0]], ast);
		resolve(ast[children[1]], ast);
	}

	void resolve_array_type(node &n, ast &ast)
	{
		assert(n.kind == node_type::ARRAY_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast[children[0]], ast);
	}

	void resolve_sum_type(node &n, ast &ast)
	{
		assert(n.kind == node_type::SUM_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);
		copy_parent_scope(n, ast);

		for (auto i = 0; i < children.size(); i += 2) resolve(ast[children[i + 1]], ast);

		for (auto i = 0; i < children.size(); i += 2)
		{
			auto &scope = ast.get_name_scope(n.name_scope_id);

			auto &id_data = ast.get_data<identifier>(ast[children[i]].data_index);
			assert(id_data.module_path.size() == 0);

			scope.define_type(id_data.name, children[i + 1]);
		}
	}

	void resolve_binary_op(node &n, ast &ast)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &lhs_node = ast[children[0]];
		resolve(lhs_node, ast);
		auto &rhs_node = ast[children[1]];
		resolve(rhs_node, ast);
	}

	void resolve_unary_op(node &n, ast &ast)
	{
		assert(ext_ast::is_unary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto &child = ast[children[0]];
		resolve(child, ast);
	}

	void register_root_fns(ast &ast)
	{
		ast_helper(ast).for_all_t(node_type::FUNCTION, [&ast](node &n) {
			auto &children = ast.children_of(n);
			auto &lhs_node = ast[children[0]];
			auto &type_node = ast[children[1]];
			lhs_node.name_scope_id = n.name_scope_id;
			type_node.name_scope_id = n.name_scope_id;
			declare_lhs(lhs_node, ast, type_node);
			define_lhs(lhs_node, ast);
		});
	}

	void resolve(node &n, ast &ast)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT: resolve_assignment(n, ast); break;
		case node_type::TUPLE: resolve_tuple(n, ast); break;
		case node_type::BLOCK: resolve_block(n, ast); break;
		case node_type::BLOCK_RESULT: resolve_block_result(n, ast); break;
		case node_type::LAMBDA: resolve_lambda(n, ast); break;
		case node_type::FUNCTION: resolve_function(n, ast); break;
		case node_type::WHILE_LOOP: resolve_while_loop(n, ast); break;
		case node_type::IF_STATEMENT: resolve_if_statement(n, ast); break;
		case node_type::MATCH_BRANCH: resolve_match_branch(n, ast); break;
		case node_type::MATCH: resolve_match(n, ast); break;
		case node_type::IDENTIFIER: resolve_id(n, ast); break;
		case node_type::FUNCTION_CALL: resolve_function_call(n, ast); break;
		case node_type::ARRAY_ACCESS: resolve_array_access(n, ast); break;
		case node_type::TYPE_DEFINITION: resolve_type_definition(n, ast); break;
		case node_type::DECLARATION: resolve_declaration(n, ast); break;
		case node_type::REFERENCE: resolve_reference(n, ast); break;
		case node_type::ARRAY_VALUE: resolve_array_value(n, ast); break;
		case node_type::ATOM_TYPE: resolve_type_atom(n, ast); break;
		case node_type::TUPLE_TYPE: resolve_type_tuple(n, ast); break;
		case node_type::FUNCTION_TYPE: resolve_function_type(n, ast); break;
		case node_type::ARRAY_TYPE: resolve_array_type(n, ast); break;
		case node_type::SUM_TYPE: resolve_sum_type(n, ast); break;
		case node_type::STRING: copy_parent_scope(n, ast); break;
		case node_type::BOOLEAN: copy_parent_scope(n, ast); break;
		case node_type::NUMBER: copy_parent_scope(n, ast); break;
		case node_type::MODULE_DECLARATION: break;
		case node_type::IMPORT_DECLARATION: break;
		default:
			if (ext_ast::is_binary_op(n.kind))
			{
				resolve_binary_op(n, ast);
				return;
			}
			if (ext_ast::is_unary_op(n.kind))
			{
				resolve_unary_op(n, ast);
				return;
			}

			assert(!"Node type not resolvable");
			throw std::runtime_error("Node type not resolvable");
		}
	}

	void resolve(ast &ast)
	{
		register_root_fns(ast);

		auto root = ast.root_id();
		resolve(ast.get_node(root), ast);
	}
} // namespace fe::ext_ast

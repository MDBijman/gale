#include "fe/data/ext_ast.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/error.h"

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
		auto& scope = ast.get_name_scope(*n.name_scope_id);

		auto& lhs_node = ast.get_node(n.children[0]);
		assert(lhs_node.data_index);
		auto& lhs_data = ast.get_data<identifier>(*lhs_node.data_index);
		assert(lhs_data.segments.size() == 1);
		auto res = scope.resolve_variable(lhs_data.segments[0]);
		assert(res);
		lhs_data.scope_distance = res->scope_distance;

		auto& value_node = ast.get_node(n.children[1]);
		resolve(value_node, ast);
	}

	void resolve_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		copy_parent_scope(n, ast);
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
		assert(id_data.segments.size() == 1);
		auto& type_name = id_data.segments[0];

		assert(n.name_scope_id);
		auto& scope = ast.get_name_scope(*n.name_scope_id);
		auto res = scope.resolve_type(type_name);
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

		parent_scope.declare_variable(id_data.segments[0], n.id);
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

	// Helper
	std::optional<std::vector<size_t>> resolve_field(node& type_node, ast& ast, const identifier& id)
	{
		copy_parent_scope(type_node, ast);
		assert(type_node.name_scope_id);
		auto& scope = ast.get_name_scope(*type_node.name_scope_id);

		if (type_node.kind == node_type::IDENTIFIER)
		{
			auto& id_data = ast.get_data<identifier>(*type_node.data_index);
			auto& res = scope.resolve_type(id_data.without_last_segment(), *id_data.segments.rbegin());
			assert(res);
			auto& referenced_type_node = ast.get_node(res->type_node);
			return resolve_field(referenced_type_node, ast, id);
		}
		else if (type_node.kind == node_type::TUPLE_DECLARATION)
		{
			size_t index = 0;
			for (auto child : type_node.children)
			{
				auto& child_node = ast.get_node(child);
				auto res = resolve_field(child_node, ast, id);
				if (res)
				{
					auto offsets = std::vector<size_t>();
					offsets.push_back(index);
					offsets.insert(offsets.end(), res->begin(), res->end());
					return offsets;
				}
				index++;
			}

			return std::nullopt;
		}
		else if (type_node.kind == node_type::ATOM_DECLARATION)
		{
			assert(type_node.children.size() == 2);
			auto& id_node = ast.get_node(type_node.children[1]);
			auto& id_data = ast.get_data<identifier>(*id_node.data_index);
			assert(id_data.segments.size() == 1);
			if (id.segments[0] == id_data.segments[0])
			{
				if (id.segments.size() > 1)
				{
					auto& new_type_node = ast.get_node(type_node.children[0]);
					return resolve_field(new_type_node, ast, id.without_first_segment());
				}
				else
				{
					return std::vector<size_t>();
				}
			}

			return std::nullopt;
		}
		else if (type_node.kind == node_type::TYPE_ATOM)
		{
			assert(id.segments.size() > 0);
			assert(type_node.children.size() == 1);
			auto& id_node = ast.get_node(type_node.children[0]);
			return resolve_field(id_node, ast, id);
		}
		else
		{
			assert(!"Unimplemented");
		}
		return std::nullopt;
	}

	std::optional<node_id> resolve_offsets(node& type_node, ast& ast, std::vector<size_t> offsets)
	{
		copy_parent_scope(type_node, ast);
		assert(type_node.name_scope_id);
		auto& scope = ast.get_name_scope(*type_node.name_scope_id);

		if (type_node.kind == node_type::IDENTIFIER)
		{
			auto& id_data = ast.get_data<identifier>(*type_node.data_index);
			auto& res = scope.resolve_type(id_data.without_last_segment(), *id_data.segments.rbegin());
			assert(res);
			auto& referenced_type_node = ast.get_node(res->type_node);
			return resolve_offsets(referenced_type_node, ast, offsets);
		}
		else if (type_node.kind == node_type::TUPLE_DECLARATION)
		{
			auto first_offset = *offsets.begin();
			auto new_offsets = std::vector<size_t>(offsets.begin() + 1, offsets.end());
			auto& new_node = ast.get_node(type_node.children.at(first_offset));
			return resolve_offsets(new_node, ast, new_offsets);
		}
		else if (type_node.kind == node_type::ATOM_DECLARATION)
		{
			assert(type_node.children.size() == 2);
			auto& id_node = ast.get_node(type_node.children[1]);
			auto& id_data = ast.get_data<identifier>(*id_node.data_index);
			assert(id_data.segments.size() == 1);
			auto& new_type_node = ast.get_node(type_node.children[0]);

			if (offsets.size() > 1)
				return resolve_offsets(new_type_node, ast, offsets);
			else
				return type_node.children[0];
		}
		else if (type_node.kind == node_type::TYPE_ATOM)
		{
			assert(offsets.size() > 0);
			assert(type_node.children.size() == 1);
			auto& id_node = ast.get_node(type_node.children[0]);
			return resolve_offsets(id_node, ast, offsets);
		}
		else
		{
			assert(!"Unimplemented");
		}
		return std::nullopt;
	}

	void resolve_id(node& n, ast& ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		copy_parent_scope(n, ast);
		auto& scope = ast.get_name_scope(*n.name_scope_id);

		assert(n.data_index);
		auto& id_data = ast.get_data<identifier>(*n.data_index);
		for (auto i = 0; i < id_data.segments.size(); i++)
		{
			identifier module(std::vector<std::string>(id_data.segments.begin(), id_data.segments.begin() + i));
			std::string name = *(id_data.segments.begin() + i);
			identifier fields(std::vector<std::string>(id_data.segments.begin() + i + 1, id_data.segments.end()));

			if (auto resolved_as_var = scope.resolve_variable(module, name); resolved_as_var)
			{
				id_data.scope_distance = resolved_as_var->scope_distance;
				if (fields.segments.size() > 1)
				{
					assert(resolved_as_var->type_node);
					auto& type_node = ast.get_node(*resolved_as_var->type_node);
					id_data.offsets = resolve_field(type_node, ast, fields).value();
				}
				else
				{
					id_data.offsets = {};
				}
				return;
			}
			else if (auto resolved_as_type = scope.resolve_type(module, name); resolved_as_type)
			{
				id_data.scope_distance = resolved_as_type->scope_distance;
				// You cannot reference a field within a type as a type name
				assert(id_data.segments.size() == 1);
				id_data.offsets = {};
				return;
			}
		}

		throw resolution_error{ std::string("Unknown identifier: ").append(id_data) };
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
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);
		assert(id_data.segments.size() == 1);

		auto& scope = ast.get_name_scope(*n.name_scope_id);

		auto& type_node = ast.get_node(n.children[1]);
		assert(type_node.kind == node_type::ATOM_DECLARATION || type_node.kind == node_type::TUPLE_DECLARATION);
		scope.define_type(id_data.segments[0], type_node.id);
	}

	// Helper for declarations
	void declare_lhs(node& lhs, ast& ast, node& type_node)
	{
		assert(lhs.kind == node_type::IDENTIFIER || lhs.kind == node_type::IDENTIFIER_TUPLE);
		copy_parent_scope(lhs, ast);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index.value());
			assert(lhs.name_scope_id);
			auto& scope = ast.get_name_scope(*lhs.name_scope_id);
			scope.declare_variable(lhs_id.segments[0], type_node.id);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			size_t i = 0;
			for (auto child : lhs.children)
			{
				auto new_type_id = resolve_offsets(type_node, ast, std::vector<size_t>{i});
				assert(new_type_id);
				auto& new_type_node = ast.get_node(*new_type_id);
				auto& new_lhs = ast.get_node(child);
				declare_lhs(new_lhs, ast, new_type_node);
				i++;
			}
		}
	}

	void define_lhs(node& lhs, ast& ast)
	{
		assert(lhs.kind == node_type::IDENTIFIER || lhs.kind == node_type::IDENTIFIER_TUPLE);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto& lhs_id = ast.get_data<identifier>(*lhs.data_index);
			assert(lhs.name_scope_id);
			auto& scope = ast.get_name_scope(*lhs.name_scope_id);
			scope.define_variable(lhs_id.segments[0]);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			for (auto child : lhs.children)
			{
				auto& new_lhs = ast.get_node(child);
				define_lhs(new_lhs, ast);
			}
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
		assert(type_node.data_index);
		declare_lhs(lhs_node, ast, type_node);

		auto& rhs_node = ast.get_node(n.children[2]);
		resolve(rhs_node, ast);

		define_lhs(lhs_node, ast);
	}

	void resolve_tuple_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE_DECLARATION);
		copy_parent_scope(n, ast);

		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			assert(child_node.kind == node_type::DECLARATION || child_node.kind == node_type::TUPLE_DECLARATION);
			resolve(child_node, ast);
		}
	}

	void resolve_atom_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_DECLARATION);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(n.children[1]);
		assert(id_node.kind == node_type::IDENTIFIER);
		assert(id_node.data_index);
		auto& node_id = ast.get_data<identifier>(*id_node.data_index);
		assert(n.name_scope_id);
		auto& scope = ast.get_name_scope(*n.name_scope_id);
		scope.declare_variable(node_id.segments[0], n.children[0]);
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

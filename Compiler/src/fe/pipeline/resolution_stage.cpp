#include "fe/data/ext_ast.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/error.h"

// Resolving
namespace fe::ext_ast
{
	// Helpers
	static void copy_parent_scope(node& n, ast& ast)
	{
		auto& parent = ast.get_node(n.parent_id);
		assert(parent.name_scope_id != no_scope);
		n.name_scope_id = parent.name_scope_id;
	}

	// Node resolvers
	void resolve_assignment(node& n, ast& ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);
		auto& scope = ast.get_name_scope(n.name_scope_id);

		auto& lhs_node = ast.get_node(children[0]);
		auto& lhs_data = ast.get_data<identifier>(lhs_node.data_index);
		assert(lhs_data.segments.size() == 1);
		auto res = scope.resolve_variable(lhs_data.segments[0], ast.name_scope_cb());
		assert(res);
		lhs_data.scope_distance = res->scope_distance;

		auto& value_node = ast.get_node(children[1]);
		resolve(value_node, ast);
	}

	void resolve_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		copy_parent_scope(n, ast);
		auto& children = ast.children_of(n);
		for (auto child : children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		auto& children = ast.children_of(n);
		if (n.parent_id)
		{
			auto& parent = ast.get_node(n.parent_id);
			n.name_scope_id = ast.create_name_scope(parent.name_scope_id);
		}
		// If we dont have a parent we already have a name scope.
		else
			assert(n.name_scope_id != no_scope);

		for (auto child : children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_block_result(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);
		auto& child = ast.get_node(children[0]);
		resolve(child, ast);
	}

	// #todo make this regular resolve?
	void declare_type_atom(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(children[0]);
		auto& id_data = ast.get_data<identifier>(id_node.data_index);
		assert(id_data.segments.size() == 1);
		auto& type_name = id_data.segments[0];

		auto& scope = ast.get_name_scope(n.name_scope_id);
		auto res = scope.resolve_type(type_name, ast.name_scope_cb());
		assert(res);
		id_data.scope_distance = res->scope_distance;
	}

	void declare_assignable(node& assignable, ast& ast)
	{
		copy_parent_scope(assignable, ast);
		if (assignable.kind == node_type::IDENTIFIER)
		{
			auto& data = ast.get_data<identifier>(assignable.data_index);
			ast.get_name_scope(assignable.name_scope_id).declare_variable(data.full, assignable.id);
			ast.get_name_scope(assignable.name_scope_id).define_variable(data.full);
		}
		else if (assignable.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto& children = ast.children_of(assignable);
			for (auto& child : children)
			{
				declare_assignable(ast.get_node(child), ast);
			}
		}
	}

	void resolve_function(node& n, ast& ast)
	{
		/*
		* A function has 2 children: assignable and expression (body)
		* We add the identifiers in the assignable to a new scope
		* And resolve the body
		*/

		assert(n.kind == node_type::FUNCTION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto& parent = ast.get_node(n.parent_id);
		n.name_scope_id = ast.create_name_scope(parent.name_scope_id);

		auto& assignable_node = ast.get_node(children.at(0));
		assert(assignable_node.kind == node_type::IDENTIFIER || assignable_node.kind == node_type::IDENTIFIER_TUPLE);

		declare_assignable(assignable_node, ast);

		// Resolve body
		resolve(ast.get_node(children[1]), ast);
	}

	void resolve_while_loop(node& n, ast& ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(children[0]), ast);
		resolve(ast.get_node(children[1]), ast);
	}

	void resolve_if_statement(node& n, ast& ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		copy_parent_scope(n, ast);
		auto& children = ast.children_of(n);
		assert(children.size() >= 2);

		// #cleanup create generic resolve_all_children
		for (auto child : children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_match_branch(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH_BRANCH);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(children[0]), ast);
		resolve(ast.get_node(children[1]), ast);
	}

	void resolve_match(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH);
		auto& children = ast.children_of(n);
		// At least an expression and a single branch
		assert(children.size() >= 2);
		copy_parent_scope(n, ast);

		for (auto child : children)
			resolve(ast.get_node(child), ast);
	}

	// Helper
	std::optional<std::vector<size_t>> resolve_field(node& type_node, ast& ast, std::vector<name> id)
	{
		copy_parent_scope(type_node, ast);
		auto& scope = ast.get_name_scope(type_node.name_scope_id);

		if (type_node.kind == node_type::IDENTIFIER)
		{
			auto& id_data = ast.get_data<identifier>(type_node.data_index);
			auto& res = scope.resolve_type({ id_data.segments.begin(), id_data.segments.end() - 1 }, 
				*id_data.segments.rbegin(), ast.name_scope_cb());
			assert(res);
			auto& referenced_type_node = ast.get_node(res->declaration_node);
			return resolve_field(referenced_type_node, ast, id);
		}
		else if (type_node.kind == node_type::RECORD)
		{
			auto& children = ast.children_of(type_node);
			size_t index = 0;
			for (auto child : children)
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
		else if (type_node.kind == node_type::RECORD_ELEMENT)
		{
			auto& children = ast.children_of(type_node);
			assert(children.size() == 2);
			auto& id_node = ast.get_node(children[0]);
			auto& id_data = ast.get_data<identifier>(id_node.data_index);
			assert(id_data.segments.size() == 1);
			if (id[0] == id_data.segments[0])
			{
				if (id.size() > 1)
				{
					auto& new_type_node = ast.get_node(children[1]);
					return resolve_field(new_type_node, ast, { id.begin() + 1, id.end() });
				}
				else
				{
					return std::vector<size_t>();
				}
			}

			return std::nullopt;
		}
		else if (type_node.kind == node_type::ATOM_TYPE)
		{
			auto& children = ast.children_of(type_node);
			assert(id.size() > 0);
			assert(children.size() == 1);
			auto& id_node = ast.get_node(children[0]);
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
		auto& children = ast.children_of(type_node);
		auto& scope = ast.get_name_scope(type_node.name_scope_id);

		if (type_node.kind == node_type::IDENTIFIER)
		{
			auto& id_data = ast.get_data<identifier>(type_node.data_index);
			auto& res = scope.resolve_type({ id_data.segments.begin(), id_data.segments.end() - 1 },
				*id_data.segments.rbegin(), ast.name_scope_cb());
			assert(res);
			auto& referenced_type_node = ast.get_node(res->declaration_node);
			return resolve_offsets(referenced_type_node, ast, offsets);
		}
		else if (type_node.kind == node_type::RECORD)
		{
			auto first_offset = *offsets.begin();
			auto new_offsets = std::vector<size_t>(offsets.begin() + 1, offsets.end());
			auto& new_node = ast.get_node(children[first_offset]);
			return resolve_offsets(new_node, ast, new_offsets);
		}
		else if (type_node.kind == node_type::RECORD_ELEMENT)
		{
			assert(children.size() == 2);
			auto& id_node = ast.get_node(children[1]);
			auto& id_data = ast.get_data<identifier>(id_node.data_index);
			assert(id_data.segments.size() == 1);
			auto& new_type_node = ast.get_node(children[0]);

			if (offsets.size() > 1)
				return resolve_offsets(new_type_node, ast, offsets);
			else
				return children[0];
		}
		else if (type_node.kind == node_type::ATOM_TYPE)
		{
			if (offsets.size() > 0)
			{
				assert(offsets.size() > 0);
				assert(children.size() == 1);
				auto& id_node = ast.get_node(children[0]);
				return resolve_offsets(id_node, ast, offsets);
			}
			else
			{
				return type_node.id;
			}
		}
		else if (type_node.kind == node_type::TUPLE_TYPE)
		{
			auto first_offset = *offsets.begin();
			auto new_offsets = std::vector<size_t>(offsets.begin() + 1, offsets.end());
			auto& new_node = ast.get_node(children[first_offset]);
			return resolve_offsets(new_node, ast, new_offsets);
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
		auto& scope = ast.get_name_scope(n.name_scope_id);

		auto& id_data = ast.get_data<identifier>(n.data_index);
		for (auto i = 0; i < id_data.segments.size(); i++)
		{
			module_name module = { id_data.segments.begin(), id_data.segments.begin() + i };
			name var_name = *(id_data.segments.begin() + i);
			std::vector<name> fields = { id_data.segments.begin() + i + 1, id_data.segments.end() };

			if (auto resolved_as_var = scope.resolve_variable(module, var_name, ast.name_scope_cb()); resolved_as_var)
			{
				id_data.scope_distance = resolved_as_var->scope_distance;
				if (fields.size() > 0)
				{
					auto type_node_id = ast.get_data<identifier>(ast.get_node(resolved_as_var->declaration_node).data_index).type_node;
					auto& type_node = ast.get_node(type_node_id);
					auto offsets = resolve_field(type_node, ast, fields);
					if (!offsets)
						throw resolution_error{ "Variable does not contain such field" };
					id_data.offsets = *offsets;
				}
				else
				{
					id_data.offsets = {};
				}
				return;
			}
			else if (auto resolved_as_type = scope.resolve_type(module, var_name, ast.name_scope_cb()); resolved_as_type)
			{
				id_data.scope_distance = resolved_as_type->scope_distance;
				id_data.offsets = {};
				return;
			}
		}

		throw resolution_error{ std::string("Unknown identifier: ").append(id_data) };
	}

	void resolve_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(children[0]), ast);
		resolve(ast.get_node(children[1]), ast);
	}
	
	void resolve_array_access(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_ACCESS);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(children[0]), ast);
		resolve(ast.get_node(children[1]), ast);
	}

	void resolve_type_definition(node& n, ast& ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto& children = ast.children_of(n);
		// identifier type_dec
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto& id_node = ast.get_node(children[0]);
		assert(id_node.kind == node_type::IDENTIFIER);
		auto& id_data = ast.get_data<identifier>(id_node.data_index);
		assert(id_data.segments.size() == 1);

		auto& scope = ast.get_name_scope(n.name_scope_id);

		auto& type_node = ast.get_node(children[1]);
		assert(type_node.kind == node_type::RECORD);
		scope.define_type(id_data.segments[0], type_node.id);
	}

	// Helper for declarations
	void declare_lhs(node& lhs, ast& ast, node& type_node)
	{
		assert(lhs.kind == node_type::IDENTIFIER || lhs.kind == node_type::IDENTIFIER_TUPLE);
		copy_parent_scope(lhs, ast);
		if (lhs.kind == node_type::IDENTIFIER)
		{
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index);
			lhs_id.type_node = type_node.id;
			auto& scope = ast.get_name_scope(lhs.name_scope_id);
			scope.declare_variable(lhs_id.segments[0], lhs.id);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			size_t i = 0;
			for (auto child : ast.children_of(lhs))
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
			auto& lhs_id = ast.get_data<identifier>(lhs.data_index);
			ast.get_name_scope(lhs.name_scope_id).define_variable(lhs_id.segments[0]);
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			for (auto child : ast.children_of(lhs))
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
		auto& children = ast.children_of(n);
		assert(children.size() == 3);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(children[0]);
		lhs_node.name_scope_id = n.name_scope_id;
		auto& type_node = ast.get_node(children[1]);
		type_node.name_scope_id = n.name_scope_id;
		resolve(type_node, ast);
		declare_lhs(lhs_node, ast, type_node);

		auto& rhs_node = ast.get_node(children[2]);
		resolve(rhs_node, ast);

		define_lhs(lhs_node, ast);
	}

	void resolve_reference(node& n, ast& ast)
	{
		assert(n.kind == node_type::REFERENCE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto& child_node = ast.get_node(children[0]);
		resolve(child_node, ast);
	}

	void resolve_array_value(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		auto& children = ast.children_of(n);
		copy_parent_scope(n, ast);

		for (auto& child : children)
			resolve(ast.get_node(child), ast);
	}

	void resolve_type_atom(node& n, ast& ast)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);
		resolve(ast.get_node(children[0]), ast);
	}

	void resolve_type_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE_TYPE);
		auto& children = ast.children_of(n);
		copy_parent_scope(n, ast);
		for (auto& child : children)
		{
			resolve(ast.get_node(child), ast);
		}
	}

	void resolve_function_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_TYPE);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(children[0]), ast);
		resolve(ast.get_node(children[1]), ast);
	}

	void resolve_array_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_TYPE);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		resolve(ast.get_node(children[0]), ast);
	}

	void resolve_sum_type(node& n, ast& ast)
	{
		assert(n.kind == node_type::SUM_TYPE);
		auto& children = ast.children_of(n);
		assert(children.size() >= 2);
		copy_parent_scope(n, ast);

		for (auto i = 0; i < children.size(); i += 2)
			resolve(ast.get_node(children[i + 1]), ast);

		for (auto i = 0; i < children.size(); i += 2)
		{
			auto& scope = ast.get_name_scope(n.name_scope_id);

			auto& id_data = ast.get_data<identifier>(ast.get_node(children[i]).data_index);
			assert(id_data.segments.size() == 1);

			scope.define_type(id_data.segments[0], children[i + 1]);
		}
	}

	void resolve_binary_op(node& n, ast& ast)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto& lhs_node = ast.get_node(children[0]);
		resolve(lhs_node, ast);
		auto& rhs_node = ast.get_node(children[1]);
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
		case node_type::ARRAY_ACCESS:      resolve_array_access(n, ast);     break;
		case node_type::TYPE_DEFINITION:   resolve_type_definition(n, ast);  break;
		case node_type::DECLARATION:       resolve_declaration(n, ast);      break;
		case node_type::REFERENCE:         resolve_reference(n, ast);        break;
		case node_type::ARRAY_VALUE:       resolve_array_value(n, ast);      break;
		case node_type::ATOM_TYPE:         resolve_type_atom(n, ast);        break;
		case node_type::TUPLE_TYPE:        resolve_type_tuple(n, ast);       break;
		case node_type::FUNCTION_TYPE:     resolve_function_type(n, ast);    break;
		case node_type::ARRAY_TYPE:        resolve_array_type(n, ast);       break;
		case node_type::SUM_TYPE:          resolve_sum_type(n, ast);         break;
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

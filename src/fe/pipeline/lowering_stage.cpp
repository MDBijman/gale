#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"

namespace fe::ext_ast
{
	core_ast::node* lower_assignment(node& n, ast& ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		assert(n.children.size() == 2);

		auto& id_node = ast.get_node(n.children[0]);
		core_ast::unique_node lhs = core_ast::unique_node(lower(id_node, ast));

		auto& value_node = ast.get_node(n.children[1]);
		core_ast::unique_node value = core_ast::unique_node(lower(value_node, ast));

		return new core_ast::set(
			*dynamic_cast<core_ast::identifier*>(lhs.get()), false,
			std::move(value), types::unique_type(new types::unset())
		);
	}

	core_ast::node* lower_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		std::vector<core_ast::unique_node> children;
		for (auto child : n.children)
		{
			children.push_back(core_ast::unique_node(lower(ast.get_node(child), ast)));
		}
		return new core_ast::tuple(std::move(children), types::unique_type(new types::unset()));
	}

	core_ast::node* lower_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		std::vector<core_ast::unique_node> children;
		for (auto child : n.children)
		{
			children.push_back(core_ast::unique_node(lower(ast.get_node(child), ast)));
		}
		return new core_ast::block(std::move(children), types::unique_type(new types::unset()));
	}

	core_ast::node* lower_function(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION);
		assert(n.children.size() == 4);

		// Name
		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.kind == node_type::IDENTIFIER);

		// Parameters
		auto& from_node = ast.get_node(n.children[1]);
		assert(from_node.kind == node_type::TUPLE_DECLARATION || from_node.kind == node_type::ATOM_DECLARATION);
		auto parameters = ([&]() -> std::variant<std::vector<core_ast::identifier>, core_ast::identifier> {
			if(from_node.kind == node_type::TUPLE_DECLARATION)
			{
				std::vector<core_ast::identifier> parameter_names;
				for (auto child : from_node.children)
				{
					auto& child_node = ast.get_node(child);
					assert(child_node.kind == node_type::ATOM_DECLARATION);
					auto& child_id_node = ast.get_node(child_node.children[1]);
					assert(child_id_node.kind == node_type::IDENTIFIER);
					core_ast::unique_node lowered_id = core_ast::unique_node(lower(child_id_node, ast));
					parameter_names.push_back(*dynamic_cast<core_ast::identifier*>(lowered_id.get()));
				}
				return parameter_names;
			}
			else if(from_node.kind == node_type::ATOM_DECLARATION)
			{
				auto& child_id_node = ast.get_node(from_node.children[1]);
				assert(child_id_node.kind == node_type::IDENTIFIER);
				core_ast::unique_node lowered_id = core_ast::unique_node(lower(child_id_node, ast));
				return *dynamic_cast<core_ast::identifier*>(lowered_id.get());
			}
		})();

		// Body
		auto& body_node = ast.get_node(n.children[3]);

		return new core_ast::function(
			std::move(*dynamic_cast<core_ast::identifier*>(core_ast::unique_node(lower(id_node, ast)).get())),
			std::move(parameters),
			core_ast::unique_node(lower(body_node, ast)),
			types::unique_type(new types::unset())
		);
	}

	core_ast::node* lower_while_loop(node& n, ast& ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);

		auto& test_node = ast.get_node(n.children[0]);
		auto& body_node = ast.get_node(n.children[1]);

		return new core_ast::while_loop(
			core_ast::unique_node(lower(test_node, ast)),
			core_ast::unique_node(lower(body_node, ast)),
			types::unset()
		);
	}

	core_ast::node* lower_if_statement(node& n, ast& ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		assert(n.children.size() == 2);

		auto& test_node = ast.get_node(n.children[0]);
		auto& body_node = ast.get_node(n.children[1]);

		std::vector<std::pair<core_ast::unique_node, core_ast::unique_node>> branches;
		branches.push_back(std::make_pair(core_ast::unique_node(lower(test_node, ast)),
			core_ast::unique_node(lower(body_node, ast))));

		return new core_ast::branch(std::move(branches));
	}

	core_ast::node* lower_match(node& n, ast& ast)
	{
		assert(n.kind == node_type::MATCH);
		std::vector<std::pair<core_ast::unique_node, core_ast::unique_node>> paths;
		for (auto child : n.children)
		{
			auto& child_node = ast.get_node(child);
			assert(child_node.kind == node_type::MATCH_BRANCH);
			assert(child_node.children.size() == 2);

			paths.push_back({
				core_ast::unique_node(lower(ast.get_node(child_node.children[0]), ast)),
				core_ast::unique_node(lower(ast.get_node(child_node.children[1]), ast))
			});
		}
		
		return new core_ast::branch(std::move(paths));
	}

	core_ast::node* lower_id(node& n, ast& ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(n.children.size() == 0);
		auto& identifier = ast.get_data<ext_ast::identifier>(n.data_index.value());
		auto modules = std::vector<std::string>(
			identifier.segments.begin(), 
			identifier.segments.end() - 1 
			);

		return new core_ast::identifier{
			std::move(modules),
			std::move(identifier.segments.at(modules.size())),
			{},
			0,
			types::unique_type(new types::unset())
		};
	}

	core_ast::node* lower_string(node& n, ast& ast)
	{
		assert(n.kind == node_type::STRING);
		assert(n.children.size() == 0);
		auto& str_data = ast.get_data<string>(n.data_index.value());
		return new core_ast::literal(values::unique_value(new values::str(str_data.value)));
	}
	core_ast::node* lower_boolean(node& n, ast& ast)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(n.children.size() == 0);
		auto& bool_data = ast.get_data<boolean>(n.data_index.value());
		return new core_ast::literal(values::unique_value(new values::boolean(bool_data.value)));
	}
	core_ast::node* lower_number(node& n, ast& ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(n.children.size() == 0);
		auto& numb_data = ast.get_data<number>(n.data_index.value());
		return new core_ast::literal(values::unique_value(new values::i64(numb_data.value)));
	}

	core_ast::node* lower_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		auto& id_node = ast.get_node(n.children[0]);
		auto lowered_id = core_ast::unique_node(lower(id_node, ast));

		auto& param_node = ast.get_node(n.children[1]);
		auto lowered_param = core_ast::unique_node(lower(param_node, ast));

		return new core_ast::function_call(
			*static_cast<core_ast::identifier*>(lowered_id.get()),
			std::move(lowered_param),
			types::unique_type(new types::unset())
		);
	}

	core_ast::node* lower_module_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(n.children.size() == 1);
		return new core_ast::no_op();
	}

	core_ast::node* lower_import_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return new core_ast::no_op();
	}

	core_ast::node* lower_export_stmt(node& n, ast& ast)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return new core_ast::no_op();
	}

	core_ast::node* lower_declaration(node& n, ast& ast)
	{
		assert(n.kind == node_type::DECLARATION);
		assert(n.children.size() == 3);

		auto& lhs_node = ast.get_node(n.children[0]);
		auto lowered_lhs = core_ast::unique_node(lower(lhs_node, ast));
		// #todo do destructuring

		auto& type_node = ast.get_node(n.children[1]);
		auto& type = ast
			.get_type_scope(*type_node.type_scope_id)
			.resolve_type(ast.get_data<identifier>(*type_node.data_index));

		auto& rhs_node = ast.get_node(n.children[2]);
		auto lowered_rhs = core_ast::unique_node(lower(rhs_node, ast));
		// #todo return core_ast::set

		return new core_ast::set(
			*dynamic_cast<core_ast::identifier*>(lowered_lhs.get()), true,
			std::move(lowered_rhs), types::unique_type(type->type.copy())
		);
	}

	core_ast::node* lower_tuple_declaration(node& n, ast& ast)
	{
		return new core_ast::no_op();
	}

	core_ast::node* lower_type_definition(node& n, ast& ast)
	{
		// #todo return core_ast::set with constructor
		return new core_ast::no_op();
	}

	core_ast::node* lower_type_atom(node& n, ast& ast)
	{
		return new core_ast::no_op();
	}

	core_ast::node* lower_reference(node& n, ast& ast)
	{
		assert(n.kind == node_type::REFERENCE);
		assert(n.children.size() == 0);
		assert(!n.data_index.has_value());
		return new core_ast::reference(core_ast::unique_node(lower(ast.get_node(n.children[0]), ast)));
	}

	core_ast::node* lower_array_value(node& n, ast& ast)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(!n.data_index.has_value());
		std::vector<core_ast::unique_node> children;

		for (auto child : n.children)
		{
			children.push_back(core_ast::unique_node(lower(ast.get_node(child), ast)));
		}

		return new core_ast::tuple(std::move(children), types::unset());
	}

	core_ast::node* lower_binary_op(node& n, ast& ast)
	{
		assert(std::find(binary_ops.begin(), binary_ops.end(), n.kind) != binary_ops.end());
		assert(n.children.size() == 2);

		if (n.kind == node_type::ADDITION)
		{
			std::vector<core_ast::unique_node> params;
			for (auto child : n.children)
				params.push_back(core_ast::unique_node(lower(ast.get_node(child), ast)));

			auto param_tuple = core_ast::unique_node(new core_ast::tuple(std::move(params), 
				types::unique_type(new types::unset())));

			return new core_ast::function_call(
				core_ast::identifier({ "_core" }, "add", {}, 0, types::unique_type(new types::i64())),
				std::move(param_tuple), types::unique_type(new types::unset())
			);
		}
		else
		{
			throw std::runtime_error("Node type not implemented");
		}
	}

	core_ast::node* lower(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:         return lower_assignment(n, ast);           break;
		case node_type::TUPLE:              return lower_tuple(n, ast);                break;
		case node_type::BLOCK:              return lower_block(n, ast);                break;
		case node_type::FUNCTION:           return lower_function(n, ast);             break;
		case node_type::WHILE_LOOP:         return lower_while_loop(n, ast);           break;
		case node_type::IF_STATEMENT:       return lower_if_statement(n, ast);         break;
		case node_type::MATCH:              return lower_match(n, ast);                break;
		case node_type::IDENTIFIER:         return lower_id(n, ast);                   break;
		case node_type::STRING:             return lower_string(n, ast);               break;
		case node_type::BOOLEAN:            return lower_boolean(n, ast);              break;
		case node_type::NUMBER:             return lower_number(n, ast);               break;
		case node_type::FUNCTION_CALL:      return lower_function_call(n, ast);        break;
		case node_type::MODULE_DECLARATION: return lower_module_declaration(n, ast);   break;
		case node_type::IMPORT_DECLARATION: return lower_import_declaration(n, ast);   break;
		case node_type::EXPORT_STMT:        return lower_export_stmt(n, ast);          break;
		case node_type::DECLARATION:        return lower_declaration(n, ast);          break;
		case node_type::TUPLE_DECLARATION:  return lower_tuple_declaration(n, ast);    break;
		case node_type::TYPE_DEFINITION:    return lower_type_definition(n, ast);      break;
		case node_type::TYPE_ATOM:          return lower_type_atom(n, ast);            break;
		case node_type::REFERENCE:          return lower_reference(n, ast);            break;
		case node_type::ARRAY_VALUE:        return lower_array_value(n, ast);          break;
		default: 
		{
			if (std::find(binary_ops.begin(), binary_ops.end(), n.kind) != binary_ops.end())
			{
				return lower_binary_op(n, ast);
			}
			else
			{
				assert(!"Node type not lowerable");
				throw std::runtime_error("Fatal Error - Node type not lowerable");
			}
		}
		}
	}
}
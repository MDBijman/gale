#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "values.h"
#include <memory>

namespace fe
{
	struct lower_error
	{

	};

	class lowering_stage : public language::lowering_stage<extended_ast::node, core_ast::node, lower_error>
	{
	public:
		std::variant<core_ast::node, lower_error> lower(extended_ast::node n) override
		{
			using namespace std;

			if (holds_alternative<extended_ast::value_tuple>(n))
			{
				auto tuple = move(get<extended_ast::value_tuple>(n));
				vector<core_ast::node> children;
				for (decltype(auto) subnode : tuple.children)
				{
					children.push_back(lower(move(subnode)));
				}

				return core_ast::node(core_ast::tuple(move(children), tuple.type));
			}
			else if (holds_alternative<extended_ast::identifier>(n))
			{
				auto& id = get<extended_ast::identifier>(n);

				return core_ast::node(core_ast::identifier(move(id.name), id.type));
			}
			else if (holds_alternative<extended_ast::assignment>(n))
			{
				auto assignment = move(get<extended_ast::assignment>(n));

				return core_ast::node(core_ast::assignment(
					core_ast::identifier(move(assignment.id.name), assignment.id.type),
					std::make_unique<core_ast::node>(lower(move(*assignment.value)))
				));
			}
			else if (holds_alternative<extended_ast::function_call>(n))
			{
				auto& fc = get<extended_ast::function_call>(n);

				return core_ast::function_call(
					core_ast::identifier(move(fc.id.name), fc.id.type),
					move(std::get<core_ast::tuple>(lower(move(fc.params)))),
					std::move(fc.type)
				);
			}
			else if (holds_alternative<extended_ast::type_declaration>(n))
			{
				auto type_declaration = move(get<extended_ast::type_declaration>(n));

				auto func_params = vector<core_ast::identifier>();
				auto& func_param_types = get<types::product_type>(type_declaration.types.type).product;

				auto return_statement = vector<core_ast::node>();
				auto return_type = vector<types::type>();

				for (unsigned int i = 0; i < func_param_types.size(); i++)
				{
					auto identifier = string("_").append(to_string(i));
					func_params.push_back(core_ast::identifier(string{ identifier }, func_param_types.at(i)));
					return_statement.push_back(core_ast::identifier(string{ identifier }, func_param_types.at(i)));
					return_type.push_back(func_param_types.at(i));
				}

				auto func_body = core_ast::make_unique(core_ast::tuple(move(return_statement), types::product_type(return_type)));

				return core_ast::node(core_ast::assignment(
					core_ast::identifier(move(type_declaration.id.name), move(type_declaration.id.type)),

					core_ast::make_unique(core_ast::function(
						values::function(
							move(func_params), 
							move(func_body)
						),
						types::function_type(
							types::make_unique(types::product_type(return_type)),
							types::make_unique(types::product_type(return_type))
						)
					))
				));
			}
			else if (holds_alternative<extended_ast::integer>(n))
			{
				return core_ast::integer(get<extended_ast::integer>(n).value);
			}
			else if (holds_alternative<extended_ast::string>(n))
			{
				return core_ast::string(get<extended_ast::string>(n).value);
			}
			else if (holds_alternative<extended_ast::bin_op>(n))
			{
				auto& operation = get<extended_ast::bin_op>(n);
				decltype(core_ast::tuple::children) children;
					
				children.push_back(std::move(lower(std::move(*operation.left))));
				children.push_back(std::move(lower(std::move(*operation.right))));
				std::string operator_name 
					= operation.operation == extended_ast::binary_operator::ADD ? "_add"
					: operation.operation == extended_ast::binary_operator::SUBTRACT ? "_subtract"
					: operation.operation == extended_ast::binary_operator::MULTIPLY ? "_multiply"
					: operation.operation == extended_ast::binary_operator::DIVIDE ? "_divide"
					: "_error";

				return core_ast::function_call(
					core_ast::identifier(
						std::move(operator_name), 
						types::product_type({ types::integer_type(), types::integer_type() })), 
					core_ast::tuple(
						std::move(children), 
						types::product_type({ types::integer_type(), types::integer_type() })
					), 
					types::integer_type()
				);
			}
			else if (holds_alternative<extended_ast::export_stmt>(n))
			{
				return core_ast::integer(1);
			}

			throw runtime_error("Unknown node type");

		}
	};
}
#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include <memory>

namespace fe
{
	class lowering_stage : public language::lowering_stage<extended_ast::node_v, std::unique_ptr<core_ast::node>>
	{
	public:


		std::unique_ptr<core_ast::node> lower(extended_ast::node_v n) override
		{
			using namespace std;
			if (holds_alternative<extended_ast::value_tuple>(n))
			{
				auto tuple = move(get<extended_ast::value_tuple>(n));
				vector<unique_ptr<core_ast::node>> children;
				for (decltype(auto) subnode : tuple.children)
				{
					children.push_back(lower(move(subnode)));
				}

				return make_unique<core_ast::tuple>(move(children), tuple.type);
			}
			else if (holds_alternative<extended_ast::identifier>(n))
			{
				auto id = move(get<extended_ast::identifier>(n));

				return make_unique<core_ast::identifier>(move(id.name), id.type);
			}
			else if (holds_alternative<extended_ast::assignment>(n))
			{
				auto assignment = move(get<extended_ast::assignment>(n));

				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(assignment.id.name), assignment.id.type),
					lower(move(*assignment.value))
				);
			}
			else if (holds_alternative<extended_ast::function_call>(n))
			{
				auto fc = move(get<extended_ast::function_call>(n));

				// HACK creating smart ptr to call lower then releasing and casting
				auto lowered_parameters = 
					dynamic_cast<core_ast::tuple*>(
						lower(move(fc.params)).release()
					);

				return make_unique<core_ast::function_call>(
					core_ast::identifier(move(fc.id.name), fc.id.type),
					move(*lowered_parameters),
					fc.type
				);
			}
			else if (holds_alternative<extended_ast::export_stmt>(n))
			{
				// HACK ? Export statement is only used for typechecking -> implement node pruning
				return nullptr;
			}
			else if (holds_alternative<extended_ast::type_declaration>(n))
			{
				auto type_declaration = move(get<extended_ast::type_declaration>(n));

				auto func_params = vector<core_ast::identifier>();
				auto& func_param_types = get<types::product_type>(type_declaration.types.type).product;

				auto return_statement = vector<unique_ptr<core_ast::node>>();
				auto return_type = vector<types::type>();

				for (unsigned int i = 0; i < func_param_types.size(); i++)
				{
					auto identifier = string("_").append(to_string(i));
					func_params.push_back(core_ast::identifier(string{ identifier }, func_param_types.at(i)));
					return_statement.push_back(make_unique<core_ast::identifier>(string{ identifier }, func_param_types.at(i)));
					return_type.push_back(func_param_types.at(i));
				}

				auto func_body = make_unique<core_ast::tuple>(move(return_statement), types::product_type(return_type));

				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(type_declaration.id.name), move(type_declaration.id.type)),

					make_unique<core_ast::function>(
						values::function(
							move(func_params), 
							move(func_body)
						),
						types::function_type(
							types::product_type(return_type),
							types::product_type(return_type)
						)
					)
				);
			}
			else if (holds_alternative<extended_ast::integer>(n))
			{
				return make_unique<core_ast::integer>(get<extended_ast::integer>(n).value);
			}
			else if (holds_alternative<extended_ast::string>(n))
			{
				return make_unique<core_ast::string>(get<extended_ast::string>(n).value);
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
					: "_error";

				return std::make_unique<core_ast::function_call>(core_ast::identifier(std::move(operator_name), types::product_type({ types::integer_type(), types::integer_type() })), core_ast::tuple(std::move(children), types::product_type({ types::integer_type(), types::integer_type() })), types::integer_type());

			}

			throw runtime_error("Unknown node type");

		}

		std::unique_ptr<core_ast::node> lower2(extended_ast::value_tuple tuple)
		{
			using namespace std;

			vector<unique_ptr<core_ast::node>> children;
			for (decltype(auto) subnode : tuple.children)
			{
				children.push_back(lower(move(subnode)));
			}

			return make_unique<core_ast::tuple>(move(children), tuple.type);
		}
	};
}
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
			if (std::holds_alternative<extended_ast::value_tuple>(n))
			{
				auto tuple = move(get<extended_ast::value_tuple>(n));
				vector<unique_ptr<core_ast::node>> children;
				for (decltype(auto) subnode : tuple.children)
				{
					children.push_back(lower(move(subnode)));
				}

				return make_unique<core_ast::tuple>(move(children), tuple.type);
			}
			else if (std::holds_alternative<extended_ast::identifier>(n))
			{
				auto id = std::move(std::get<extended_ast::identifier>(n));

				return make_unique<core_ast::identifier>(move(id.name), id.type);
			}
			else if (std::holds_alternative<extended_ast::assignment>(n))
			{
				auto assignment = std::move(std::get<extended_ast::assignment>(n));

				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(assignment.id.name), assignment.id.type),
					lower(move(assignment.value))
				);
			}
			else if (std::holds_alternative<extended_ast::function_call>(n))
			{
				auto fc = std::move(std::get<extended_ast::function_call>(n));

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
			else if (std::holds_alternative<extended_ast::export_stmt>(n))
			{
				// HACK ? Export statement is only used for typechecking -> implement node pruning
				return nullptr;
			}
			else if (std::holds_alternative<extended_ast::type_declaration>(n))
			{
				auto type_declaration = std::move(std::get<extended_ast::type_declaration>(n));

				auto func_params = std::vector<core_ast::identifier>();
				func_params.push_back(core_ast::identifier("1", types::integer_type()));

				auto func_body = std::vector<std::unique_ptr<core_ast::node>>();
				func_body.push_back(make_unique<core_ast::integer>(values::integer(1)));

				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(type_declaration.id.name), move(type_declaration.id.type)),

					make_unique<core_ast::function>(
						values::function(
							move(func_params), 
							make_unique<core_ast::tuple>(
								move(func_body),
								types::product_type({ types::integer_type() })
							)
						),
						types::function_type(
							types::product_type({ types::integer_type() }),
							types::product_type({ types::integer_type() })
						)
					)
				);
			}
			else if (std::holds_alternative<extended_ast::integer>(n))
			{
				return make_unique<core_ast::integer>(std::get<extended_ast::integer>(n).value);
			}
			else if (std::holds_alternative<extended_ast::string>(n))
			{
				return make_unique<core_ast::string>(std::get<extended_ast::string>(n).value);
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
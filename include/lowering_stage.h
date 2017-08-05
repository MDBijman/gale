#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include <memory>

namespace fe
{
	class lowering_stage : public language::lowering_stage<extended_ast::node_p, std::unique_ptr<core_ast::node>>
	{
	public:
		std::unique_ptr<core_ast::node> lower(extended_ast::node_p n) override
		{
			using namespace std;

			if (auto tuple = dynamic_cast<extended_ast::value_tuple*>(n.get()))
			{
				std::vector<std::unique_ptr<core_ast::node>> children;
				for (decltype(auto) subnode : tuple->children)
				{
					children.push_back(lower(move(subnode)));
				}

				auto nl = make_unique<core_ast::tuple>(std::move(children), tuple->type);
				return nl;
			}
			else if (auto id = dynamic_cast<extended_ast::identifier*>(n.get()))
			{
				return make_unique<core_ast::identifier>(move(id->name), id->type);
			}
			else if (auto assignment = dynamic_cast<extended_ast::assignment*>(n.get()))
			{
				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(assignment->id.name), assignment->id.type),
					lower(move(assignment->value))
				);
			}
			else if (auto fc = dynamic_cast<extended_ast::function_call*>(n.get()))
			{
				// HACK creating smart ptr to call lower then releasing and casting
				auto lowered_parameters = 
					dynamic_cast<core_ast::tuple*>(
						lower(make_unique<extended_ast::value_tuple>(move(fc->params))).release()
					);

				return make_unique<core_ast::function_call>(
					core_ast::identifier(move(fc->id.name), fc->id.type),
					move(*lowered_parameters),
					fc->type
				);
			}
			else if (auto export_stmt = dynamic_cast<extended_ast::export_stmt*>(n.get()))
			{
				// HACK ? Export statement is only used for typechecking -> implement node pruning
				return nullptr;
			}
			else if (auto type_declaration = dynamic_cast<extended_ast::type_declaration*>(n.get()))
			{
				auto func_params = std::vector<core_ast::identifier>();
				func_params.push_back(core_ast::identifier("1", types::integer_type()));

				auto func_body = std::vector<std::unique_ptr<core_ast::node>>();
				func_body.push_back(make_unique<core_ast::integer>(values::integer(1)));

				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(type_declaration->id.name), move(type_declaration->id.type)),

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
			else if (auto integer = dynamic_cast<extended_ast::integer*>(n.get()))
			{
				return make_unique<core_ast::integer>(integer->value);
			}
			else if (auto string = dynamic_cast<extended_ast::string*>(n.get()))
			{
				return make_unique<core_ast::string>(string->value);
			}

			throw runtime_error("Unknown node type");
		}
	};
}
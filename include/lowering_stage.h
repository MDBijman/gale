#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include <memory>

namespace fe
{
	class lowering_stage : public language::lowering_stage<extended_ast::node_p, std::unique_ptr<core_ast::node>>
	{
	public:
		std::unique_ptr<core_ast::node> lower(extended_ast::node_p n)
		{
			using namespace std;

			if (auto tuple = dynamic_cast<extended_ast::tuple*>(n.get()))
			{
				auto nl = make_unique<core_ast::tuple>();
				for (decltype(auto) subnode : tuple->get_children())
				{
					(*nl).children.push_back(lower(move(subnode)));
				}

				return nl;
			}
			else if (auto id = dynamic_cast<extended_ast::identifier*>(n.get()))
			{
				return make_unique<core_ast::identifier>(move(id->name));
			}
			else if (auto assignment = dynamic_cast<extended_ast::assignment*>(n.get()))
			{
				return make_unique<core_ast::assignment>(
					core_ast::identifier(move(assignment->id.name)),
					move(lower(move(assignment->value)))
				);
			}
			else if (auto fc = dynamic_cast<extended_ast::function_call*>(n.get()))
			{
				// HACK creating smart ptr to call lower then releasing and casting
				auto lowered_parameters = 
					dynamic_cast<core_ast::tuple*>(
						lower(make_unique<extended_ast::tuple>(move(fc->params))).release()
					);

				return make_unique<core_ast::function_call>(
					core_ast::identifier(move(fc->id.name)),
					move(*lowered_parameters)
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
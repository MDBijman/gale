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
			if (auto tuple = dynamic_cast<extended_ast::tuple*>(n.get()))
			{
				auto nl = std::make_unique<core_ast::tuple>();
				for (decltype(auto) subnode : tuple->get_children())
				{
					(*nl).children.push_back(lower(std::move(subnode)));
				}

				return nl;
			}
			else if (auto id = dynamic_cast<extended_ast::identifier*>(n.get()))
			{
				return std::make_unique<core_ast::identifier>(std::move(id->name));
			}
			else if (auto assignment = dynamic_cast<extended_ast::assignment*>(n.get()))
			{
				return std::make_unique<core_ast::assignment>(
					core_ast::identifier(std::move(assignment->id.name)),
					std::move(lower(std::move(assignment->value)))
				);
			}
			else if (auto fc = dynamic_cast<extended_ast::function_call*>(n.get()))
			{
				auto lowered_parameters =
					dynamic_cast<core_ast::tuple*>(lower(std::make_unique<extended_ast::tuple>(std::move(fc->params))).release());

				return std::make_unique<core_ast::function_call>(
					core_ast::identifier(std::move(fc->id.name)),
					std::move(*lowered_parameters)
				);
			}
			else if (auto constructor = dynamic_cast<extended_ast::constructor*>(n.get()))
			{
				auto id = dynamic_cast<core_ast::identifier*>(lower(std::make_unique<extended_ast::identifier>(std::move(constructor->id))).release());
				auto tuple = dynamic_cast<core_ast::tuple*>(lower(std::make_unique<extended_ast::tuple>(std::move(constructor->value))).release());

				return std::make_unique<core_ast::constructor>(
					std::move(*id),
					std::move(*tuple)
				);
			}
			else if (auto integer = dynamic_cast<extended_ast::integer*>(n.get()))
			{
				return std::make_unique<core_ast::integer>(integer->value);
			}
			else if (auto string = dynamic_cast<extended_ast::string*>(n.get()))
			{
				return std::make_unique<core_ast::string>(string->value);
			}

			throw std::runtime_error("Unknown node type");
		}
	};
}
#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "values.h"
#include "error.h"

#include <memory>

namespace fe
{

	class lowering_stage : public language::lowering_stage<extended_ast::unique_node, core_ast::unique_node, lower_error>
	{
	public:
		std::variant<core_ast::unique_node, lower_error> lower(extended_ast::unique_node n) override
		{
			try
			{
				return core_ast::unique_node(n->lower());
			}
			catch (lower_error e)
			{
				return e;
			}
		}
	};
}
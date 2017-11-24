#pragma once
#include "utils/parsing/ebnfe_parser.h"
#include "fe/data/extended_ast.h"
#include "fe/data/core_ast.h"
#include "fe/data/values.h"

#include "error.h"

#include <memory>

namespace fe
{
	class lowering_stage 
	{
	public:
		std::variant<core_ast::unique_node, lower_error> lower(extended_ast::unique_node n) const
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
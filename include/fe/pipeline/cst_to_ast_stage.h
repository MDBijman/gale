#pragma once
#include "error.h"
#include "fe/data/ext_ast.h"
#include "utils/parsing/ebnfe_parser.h"

#include <string_view>
#include <assert.h>

namespace fe
{
	class cst_to_ast_stage 
	{
	public:
		std::variant<ext_ast::ast, cst_to_ast_error> convert(std::unique_ptr<utils::ebnfe::node> node) const
		{
			try
			{
				return conv(std::move(node));
			}
			catch (cst_to_ast_error e)
			{
				return e;
			}
		}

		ext_ast::ast conv(std::unique_ptr<utils::ebnfe::node> node) const;
	};
}
#pragma once
#include "ast.h"
#include <memory>

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<std::unique_ptr<ast::node>, std::unique_ptr<ast::node>>
	{
	public:
		std::unique_ptr<ast::node> typecheck(std::unique_ptr<ast::node> ast) override
		{
			return std::move(ast);
		}
	};
}
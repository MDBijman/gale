#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"
#include "error.h"
#include "typecheck_environment.h"
#include "tags.h"

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<extended_ast::unique_node, extended_ast::unique_node, typecheck_environment, typecheck_error>
	{
	private:
		typecheck_environment base_environment;
	
	public:
		typechecker_stage() {}
		typechecker_stage(typecheck_environment environment) : base_environment(environment) {}

		std::variant<std::tuple<extended_ast::unique_node, typecheck_environment>, typecheck_error> typecheck(extended_ast::unique_node node, typecheck_environment env) override
		{
			try
			{
				node->typecheck(env);
			}
			catch (typecheck_error e)
			{
				return e;
			}

			return std::make_tuple(std::move(node), env);
		}
	};
}
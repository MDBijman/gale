#pragma once
#include "ebnfe_parser.h"
#include "language_definition.h"
#include "values.h"
#include "pipeline.h"
#include "core_ast.h"
#include "runtime_environment.h"
#include "error.h"

#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	class interpreting_stage : public language::interpreting_stage<core_ast::unique_node, values::value, runtime_environment, interp_error>
	{
	public:
		interpreting_stage() {}

		std::variant<
			std::tuple<core_ast::unique_node, values::value, runtime_environment>,
			interp_error
		> interpret(core_ast::unique_node core_ast, runtime_environment&& env) override
		{
			try
			{
				auto res = core_ast->interp(env);
				return std::make_tuple(std::move(core_ast), res, env);
			}
			catch (interp_error e)
			{
				return e;
			}
		}

	};
}
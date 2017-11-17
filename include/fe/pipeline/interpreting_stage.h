#pragma once
#include "utils/parsing/ebnfe_parser.h"
#include "fe/language_definition.h"
#include "fe/data/values.h"
#include "fe/data/core_ast.h"
#include "fe/data/runtime_environment.h"

#include "error.h"
#include "pipeline.h"

#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	class interpreting_stage : public language::interpreting_stage<core_ast::unique_node, values::unique_value, runtime_environment, interp_error>
	{
	public:
		interpreting_stage() {}

		std::variant<
			std::tuple<core_ast::unique_node, values::unique_value, runtime_environment>,
			interp_error
		> interpret(core_ast::unique_node core_ast, runtime_environment&& env) override
		{
			try
			{
				auto res = core_ast->interp(env);
				return std::make_tuple(std::move(core_ast), std::move(res), env);
			}
			catch (interp_error e)
			{
				return e;
			}
		}

	};
}
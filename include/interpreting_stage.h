#pragma once
#include "ebnfe_parser.h"
#include "language_definition.h"
#include "values.h"
#include "pipeline.h"
#include "core_ast.h"
#include "error.h"

#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	class interpreting_stage : public language::interpreting_stage<core_ast::node, values::value, environment, interp_error>
	{
	public:
		interpreting_stage() {}

		std::variant<
			std::tuple<core_ast::node, values::value, environment>,
			interp_error
		> interpret(core_ast::node core_ast, environment&& env) override
		{
			using namespace std;
			if (holds_alternative<core_ast::no_op>(core_ast))
			{
				return make_tuple(move(core_ast), values::value(values::void_value()), move(env));
			}
			else if (holds_alternative<core_ast::tuple>(core_ast))
			{
				auto& tuple_node = get<core_ast::tuple>(core_ast);
				values::tuple res = values::tuple();
				 
				for (auto& line : tuple_node.children)
				{
					auto interpreted_line = interpret(move(line), move(env));
					if (holds_alternative<interp_error>(interpreted_line))
						return get<interp_error>(interpreted_line);

					auto[new_line, line_value, new_env] = std::move(get<tuple<core_ast::node, values::value, environment>>(interpreted_line));
					line = move(new_line);
					env = move(new_env);

					res.content.push_back(line_value);
				}

				return make_tuple(move(tuple_node), move(res), move(env));

			}
			else if (holds_alternative<core_ast::identifier>(core_ast))
			{
				auto& id = get<core_ast::identifier>(core_ast);
				auto& id_value = env.valueof(id.name);

				return make_tuple(move(id), id_value, move(env));
			}
			else if (holds_alternative<core_ast::assignment>(core_ast))
			{
				auto& assignment = get<core_ast::assignment>(core_ast);

				auto interpreted_value = std::move(interpret(move(*assignment.value), move(env)));
				if (holds_alternative<interp_error>(interpreted_value))
					return get<interp_error>(interpreted_value);

				auto&[new_code, new_value, new_env] = get<tuple<core_ast::node, values::value, environment>>(interpreted_value);
				assignment.value = make_unique<core_ast::node>(move(new_code));
				env = move(new_env);

				env.set_value(assignment.id.name, move(new_value));

				return make_tuple(move(core_ast), env.valueof(assignment.id.name), move(env));
			}
			else if (holds_alternative<core_ast::function_call>(core_ast))
			{
				auto& call = get<core_ast::function_call>(core_ast);
				auto& function_value = env.valueof(call.id.name);

				fe::environment param_env{ env };
				auto res_or_error = interpret(std::move(*call.parameter), std::move(param_env));
				if (std::holds_alternative<interp_error>(res_or_error))
					return std::get<interp_error>(res_or_error);
				auto& res = std::get<std::tuple<core_ast::node, values::value, environment>>(res_or_error);

				call.parameter = core_ast::make_unique(std::get<core_ast::node>(res));
				auto& param_value = std::get<values::value>(res);

				// Function written in cpp
				if (holds_alternative<values::native_function>(function_value))
				{
					auto& function = get<values::native_function>(function_value);

					// Eval function
					auto result = move(function.function(move(param_value)));

					return make_tuple(move(core_ast), move(result), move(env));
				}
				// Function written in language itself
				else if (holds_alternative<values::function>(function_value))
				{
					auto& function = *get<values::function>(function_value).func;

					// TODO improve
					fe::environment function_env;
					if (call.id.name.size() > 1)
						function_env = env.get_module(call.id.name.at(0));
					else
						function_env = env;

					auto& param_tuple = std::get<values::tuple>(param_value);
					for(auto i = 0; i < function.parameters.size(); i++)
					{
						const std::vector<std::string>& name = function.parameters.at(i).name;
						function_env.set_value(std::vector<std::string>(name), std::move(param_tuple.content.at(i)));
					}

					auto interpreted_value = interpret(*function.body, move(function_env));
					if (holds_alternative<interp_error>(interpreted_value))
						return get<interp_error>(interpreted_value);

					auto [_, result, _] = std::move(get<tuple<core_ast::node, values::value, environment>>(interpreted_value));

					return make_tuple(move(core_ast), move(result), move(env));
				}

				return interp_error{ "Function call calls non-function type value" };
			}
			else if (holds_alternative<core_ast::integer>(core_ast))
			{
				auto& num = get<core_ast::integer>(core_ast);
				return make_tuple(move(core_ast), values::integer(num.value), move(env));
			}
			else if (holds_alternative<core_ast::string>(core_ast))
			{
				auto& string = get<core_ast::string>(core_ast);
				return make_tuple(move(core_ast), values::string(string.value), move(env));
			}
			else if (holds_alternative<core_ast::function>(core_ast))
			{
				auto& function = get<core_ast::function>(core_ast);
				auto value_function = function;
				return make_tuple(move(core_ast), values::function(std::make_unique<core_ast::function>(std::move(value_function))), move(env));
			}
			else if (holds_alternative<core_ast::conditional_branch>(core_ast))
			{
				auto& conditional_branch = std::get<core_ast::conditional_branch>(core_ast);

				for (decltype(auto) branch : conditional_branch.branches)
				{
					auto interpreted_test_or_error = interpret(std::move(*branch.test_path), std::move(env));
					if (std::holds_alternative<interp_error>(interpreted_test_or_error))
						std::get<interp_error>(interpreted_test_or_error);
					auto& interpreted_test = std::get<std::tuple<core_ast::node, values::value, environment>>(interpreted_test_or_error);
					env = std::move(std::get<environment>(interpreted_test));
					branch.test_path = core_ast::make_unique(std::move(std::get<core_ast::node>(interpreted_test)));

					auto& value = std::get<values::value>(interpreted_test);
					if (std::get<values::boolean>(value).val)
					{
						auto interpreted_code_or_error = interpret(std::move(*branch.code_path), std::move(env));
						if (std::holds_alternative<interp_error>(interpreted_code_or_error))
							std::get<interp_error>(interpreted_code_or_error);
						auto& interpreted_code = std::get<std::tuple<core_ast::node, values::value, environment>>(interpreted_code_or_error);
						env = std::move(std::get<environment>(interpreted_code));
						branch.code_path = core_ast::make_unique(std::move(std::get<core_ast::node>(interpreted_code)));
						
						return make_tuple(move(core_ast), std::get<values::value>(interpreted_code), std::move(env));
					}
				}

				return interp_error{ "None of the branches are evaluated" };
			}

			return interp_error{ "Unknown core node" };
		}
	};
}
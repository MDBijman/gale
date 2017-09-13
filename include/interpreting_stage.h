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
	class interpreting_stage : public language::interpreting_stage<core_ast::node, values::value, runtime_environment, interp_error>
	{
	public:
		interpreting_stage() {}

		std::variant<
			std::tuple<core_ast::node, values::value, runtime_environment>,
			interp_error
		> interpret(core_ast::node core_ast, runtime_environment&& env) override
		{
			auto visitor = [&](auto&& node) {
				return interpret(std::move(node), std::move(env));
			};

			return std::visit(visitor, std::move(core_ast));
		}

		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::no_op&& no_op, runtime_environment&& env)
		{
			return make_tuple(std::move(no_op), values::value(values::void_value()), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::tuple&& tuple_node, runtime_environment&& env)
		{
			values::tuple res = values::tuple();

			for (auto& line : tuple_node.children)
			{
				auto interpreted_line = interpret(std::move(line), std::move(env));
				if (std::holds_alternative<interp_error>(interpreted_line))
					return std::get<interp_error>(interpreted_line);

				auto& [new_line, line_value, new_env] = std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_line);
				line = std::move(new_line);
				env = std::move(new_env);

				res.content.push_back(line_value);
			}

			return std::make_tuple(std::move(tuple_node), std::move(res), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::block&& block, runtime_environment&& env)
		{
			values::value res;

			for (auto& exp : block.children)
			{
				auto interpreted_exp = interpret(std::move(exp), std::move(env));
				if (std::holds_alternative<interp_error>(interpreted_exp))
					return std::get<interp_error>(interpreted_exp);

				auto[new_exp, exp_value, new_env] = std::move(std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_exp));
				exp = std::move(new_exp);
				env = std::move(new_env);

				res = exp_value;
			}

			return std::make_tuple(std::move(block), std::move(res), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::identifier&& id, runtime_environment&& env)
		{
			auto& id_value = env.valueof(id);

			return make_tuple(std::move(id), id_value, std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::set&& setter, runtime_environment&& env)
		{
			auto interpreted_value = std::move(interpret(std::move(*setter.value), std::move(env)));
			if (std::holds_alternative<interp_error>(interpreted_value))
				return std::get<interp_error>(interpreted_value);

			auto& [new_code, new_value, new_env] = std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_value);
			setter.value = std::make_unique<core_ast::node>(move(new_code));
			env = std::move(new_env);

			env.set_value(setter.id.variable_name, move(new_value));

			return make_tuple(std::move(setter), env.valueof(setter.id), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::function_call&& call, runtime_environment&& env)
		{
			auto& function_value = env.valueof(call.id);

			runtime_environment param_env{ env };
			auto res_or_error = interpret(std::move(*call.parameter), std::move(param_env));
			if (std::holds_alternative<interp_error>(res_or_error))
				return std::get<interp_error>(res_or_error);
			auto& res = std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(res_or_error);

			call.parameter = core_ast::make_unique(std::get<core_ast::node>(res));
			auto& param_value = std::get<values::value>(res);

			// Function written in cpp
			if (std::holds_alternative<values::native_function>(function_value))
			{
				auto& function = std::get<values::native_function>(function_value);

				// Eval function
				auto result = std::move(function.function(move(param_value)));

				return std::make_tuple(std::move(call), std::move(result), std::move(env));
			}
			// Function written in language itself
			else if (std::holds_alternative<values::function>(function_value))
			{
				auto& function = *std::get<values::function>(function_value).func;

				// TODO improve
				runtime_environment function_env;
				if (call.id.modules.size() > 0)
					function_env = env.get_module(call.id.modules.at(0));
				else
					function_env = env;

				auto& param_tuple = std::get<values::tuple>(param_value);

				if (std::holds_alternative<std::vector<core_ast::identifier>>(function.parameters))
				{
					auto& parameters = std::get<std::vector<core_ast::identifier>>(function.parameters);
					for (auto i = 0; i < parameters.size(); i++)
					{
						auto& id = parameters.at(i);
						function_env.set_value(id.variable_name, std::move(param_tuple.content.at(i)));
					}
				}
				else
				{
					// Single argument function
					auto& parameter = std::get<core_ast::identifier>(function.parameters);
					function_env.set_value(parameter.variable_name, std::move(param_tuple));
				}

				auto interpreted_value = interpret(*function.body, std::move(function_env));
				if (std::holds_alternative<interp_error>(interpreted_value))
					return std::get<interp_error>(interpreted_value);

				auto[_, result, _] = std::move(std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_value));

				return std::make_tuple(std::move(call), std::move(result), std::move(env));
			}

			return interp_error{ "Function call calls non-function type value" };
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::integer&& num, runtime_environment&& env)
		{
			return std::make_tuple(std::move(num), values::integer(num.value), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::string&& string, runtime_environment&& env)
		{
			return std::make_tuple(std::move(string), values::string(string.value), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::function&& function, runtime_environment&& env)
		{
			auto value_function = function;
			return std::make_tuple(std::move(function), values::function(std::make_unique<core_ast::function>(std::move(value_function))), std::move(env));
		}
		std::variant<std::tuple<core_ast::node, values::value, runtime_environment>, interp_error> interpret(core_ast::conditional_branch&& conditional_branch, runtime_environment&& env)
		{
			for (decltype(auto) branch : conditional_branch.branches)
			{
				auto interpreted_test_or_error = interpret(std::move(*branch.test_path), std::move(env));
				if (std::holds_alternative<interp_error>(interpreted_test_or_error))
					std::get<interp_error>(interpreted_test_or_error);
				auto& interpreted_test = std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_test_or_error);
				env = std::move(std::get<runtime_environment>(interpreted_test));
				branch.test_path = core_ast::make_unique(std::move(std::get<core_ast::node>(interpreted_test)));

				auto& value = std::get<values::value>(interpreted_test);
				if (std::get<values::boolean>(value).val)
				{
					auto interpreted_code_or_error = interpret(std::move(*branch.code_path), std::move(env));
					if (std::holds_alternative<interp_error>(interpreted_code_or_error))
						std::get<interp_error>(interpreted_code_or_error);
					auto& interpreted_code = std::get<std::tuple<core_ast::node, values::value, runtime_environment>>(interpreted_code_or_error);
					env = std::move(std::get<runtime_environment>(interpreted_code));
					branch.code_path = core_ast::make_unique(std::move(std::get<core_ast::node>(interpreted_code)));

					return make_tuple(std::move(conditional_branch), std::get<values::value>(interpreted_code), std::move(env));
				}
			}

			return interp_error{ "None of the conditional branches are evaluated" };
		}
	};
}
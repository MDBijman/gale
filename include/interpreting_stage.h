#pragma once
#include "ebnfe_parser.h"
#include "language_definition.h"
#include "values.h"
#include "pipeline.h"
#include "core_ast.h"
#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	struct interp_error
	{

	};

	class interpreting_stage : public language::interpreting_stage<core_ast::node, values::value, environment>
	{
	public:
		interpreting_stage() {}

		std::tuple<
			core_ast::node,
			values::value,
			environment
		> interpret(core_ast::node core_ast, environment&& env) override
		{
			using namespace std;
			if (std::holds_alternative<core_ast::tuple>(core_ast))
			{
				auto& tuple_node = std::get<core_ast::tuple>(core_ast);
				values::tuple res = values::tuple();
				 
				for (auto& line : tuple_node.children)
				{
					auto [new_line, line_value, new_env] = interpret(move(line), move(env));
					line = std::move(new_line);
					env = std::move(new_env);

					res.content.push_back(line_value);
				}

				return make_tuple(move(tuple_node), move(res), move(env));

			}
			else if (std::holds_alternative<core_ast::identifier>(core_ast))
			{
				auto& id = std::get<core_ast::identifier>(core_ast);
				auto& id_value = env.valueof(id.name);

				return make_tuple(move(id), id_value, move(env));
			}
			else if (std::holds_alternative<core_ast::assignment>(core_ast))
			{
				auto& assignment = std::get<core_ast::assignment>(core_ast);

				auto [new_code, new_value, new_env] = interpret(move(*assignment.value), move(env));
				assignment.value = std::make_unique<core_ast::node>(std::move(new_code));
				env = std::move(new_env);

				env.set_value(assignment.id.name, std::move(new_value));

				return make_tuple(move(core_ast), env.valueof(assignment.id.name), move(env));
			}
			else if (std::holds_alternative<core_ast::function_call>(core_ast))
			{
				auto& call = std::get<core_ast::function_call>(core_ast);
				auto& value = env.valueof(call.id.name);

				// Function written in cpp
				if (std::holds_alternative<values::native_function>(value))
				{
					auto& function = std::get<values::native_function>(value);

					// Eval parameters
					std::vector<values::value> params;
					for (unsigned int i = 0; i < call.params.children.size(); i++)
					{
						fe::environment param_env{ env };
						auto [new_code, new_value, _] = interpret(move(call.params.children.at(i)), move(param_env));
						call.params.children.at(i) = std::move(new_code);
						params.push_back(std::move(new_value));
					}
					values::tuple param_tuple(std::move(params));

					// Eval function
					auto result = std::move(function.function(std::move(param_tuple)));

					return make_tuple(move(core_ast), std::move(result), move(env));
				}
				// Function written in language itself
				else if (std::holds_alternative<values::function>(value))
				{
					auto function = std::get<values::function>(value);

					fe::environment function_env{ env };

					for (unsigned int i = 0; i < call.params.children.size(); i++)
					{
						fe::environment param_env{ env };

						auto[new_code, new_value, _] = interpret(move(call.params.children.at(i)), move(param_env));
						call.params.children.at(i) = std::move(new_code);
						std::move(core_ast::tuple({}, types::unset_type()));
						auto param_name = function.parameters.at(i).name;

						function_env.set_value(param_name, std::move(new_value));
					}

					auto [_, result, _] = interpret(move(*function.body), move(function_env));

					return make_tuple(move(core_ast), std::move(result), move(env));
				}
			}
			else if (std::holds_alternative<core_ast::integer>(core_ast))
			{
				auto& num = std::get<core_ast::integer>(core_ast);
				return make_tuple(move(core_ast), values::integer(num.value), move(env));
			}
			else if (std::holds_alternative<core_ast::string>(core_ast))
			{
				auto& string = std::get<core_ast::string>(core_ast);
				return make_tuple(move(core_ast), values::string(string.value), move(env));
			}
			else if (std::holds_alternative<core_ast::function>(core_ast))
			{
				auto& function = std::get<core_ast::function>(core_ast);
				return make_tuple(move(core_ast), values::function(move(function.fun)), move(env));
			}

			throw runtime_error("Unknown AST node");
		}
	};
}
#pragma once
#include "ebnfe_parser.h"
#include "language_definition.h"
#include "values.h"
#include "pipeline.h"
#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<core_ast::node>, std::shared_ptr<fe::values::value>, fe::environment>
	{
	public:
		interpreting_stage() {}

		std::tuple<
			std::unique_ptr<core_ast::node>,
			std::shared_ptr<values::value>,
			fe::environment
		> interpret(std::unique_ptr<core_ast::node> core_ast, fe::environment&& env) override
		{
			using namespace std;

			if (!core_ast)
			{
				return make_tuple(move(core_ast), make_shared<values::void_value>(), move(env));
			}
			else if (auto tuple_node = dynamic_cast<core_ast::tuple*>(core_ast.get()))
			{
				shared_ptr<values::value> res;
				for (decltype(auto) line : tuple_node->children)
				{
					tie(line, res, env) = interpret(move(line), move(env));
				}

				return make_tuple(move(core_ast), move(res), move(env));
			}
			else if (auto id = dynamic_cast<core_ast::identifier*>(core_ast.get()))
			{
				auto interpreted_id = env.valueof(id->name);
				return make_tuple(move(core_ast), interpreted_id, move(env));
			}
			else if (auto assignment = dynamic_cast<core_ast::assignment*>(core_ast.get()))
			{
				shared_ptr<values::value> value;
				tie(assignment->value, value, env) = interpret(move(assignment->value), move(env));

				env.set_value(assignment->id.name, value);

				return make_tuple(move(core_ast), env.valueof(assignment->id.name), move(env));
			}
			else if (auto call = dynamic_cast<core_ast::function_call*>(core_ast.get()))
			{
				auto function = dynamic_cast<values::function*>(env.valueof(call->id.name).get());
				fe::environment function_env{ env };

				for (unsigned int i = 0; i < call->params.children.size(); i++)
				{
					shared_ptr<values::value> param_value;
					tie(call->params.children.at(i), param_value, std::ignore) 
						= interpret(move(call->params.children.at(i)), move(env));

					auto param_name = function->parameters.at(i).name;
					function_env.set_value(param_name, param_value);
				}

				shared_ptr<values::value> value;
				tie(function->body, value, env) = interpret(move(function->body), move(function_env));
				return make_tuple(move(core_ast), value, move(env));
			}
			else if (auto num = dynamic_cast<core_ast::integer*>(core_ast.get()))
			{
				return make_tuple(move(core_ast), make_shared<values::integer>(num->value), move(env));
			}
			else if (auto string = dynamic_cast<core_ast::string*>(core_ast.get()))
			{
				return make_tuple(move(core_ast), make_shared<values::string>(string->value), move(env));
			}
			else if (auto function = dynamic_cast<core_ast::function*>(core_ast.get()))
			{
				return make_tuple(move(core_ast), make_shared<values::function>(move(function->fun.parameters), move(function->fun.body)), move(env));
			}

			throw runtime_error("Unknown AST node");
		}
	};
}
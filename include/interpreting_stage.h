#pragma once
#include "ebnfe_parser.h"
#include "language_definition.h"
#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<core_ast::node>, std::shared_ptr<values::value>, fe::environment>
	{
	private:
		value_environment base_environment;

	public:
		interpreting_stage() {}
		interpreting_stage(value_environment base_environment) : base_environment(base_environment) {}

		std::shared_ptr<values::value> interpret(std::unique_ptr<core_ast::node> core_ast, fe::environment&& env) override
		{
			env.v_env = value_environment{base_environment};
			auto env_p = std::make_unique<fe::environment>(std::move(env));

			std::shared_ptr<values::value> result;
			std::tie(result, env_p) = interpret(core_ast.get(), std::move(env_p));

			return result;
		}

		std::tuple<
			std::shared_ptr<values::value>, 
			std::unique_ptr<fe::environment>
		> interpret(core_ast::node* core_ast, std::unique_ptr<fe::environment> env)
		{
			using namespace std;

			if (auto tuple_node = dynamic_cast<core_ast::tuple*>(core_ast))
			{
				shared_ptr<values::value> res;
				for (decltype(auto) line : tuple_node->children)
				{
					tie(res, env->v_env) = interpret(line.get(), move(env));
				}

				return make_tuple(move(res), move(env));
			}
			else if (auto id = dynamic_cast<core_ast::identifier*>(core_ast))
			{
				auto interpreted_id = env->v_env.get(id->name);
				return make_tuple(interpreted_id, move(env));
			}
			else if (auto assignment = dynamic_cast<core_ast::assignment*>(core_ast))
			{
				shared_ptr<values::value> value;
				tie(value, env) = move(interpret(assignment->value.get(), move(env)));

				env->v_env.set(assignment->id.name, value);

				return make_tuple(env->v_env.get(assignment->id.name), move(env));
			}
			else if (auto call = dynamic_cast<core_ast::function_call*>(core_ast))
			{

				auto function = dynamic_cast<values::function*>(env->v_env.get(call->id.name).get());
				auto function_env = make_unique<environment>(*env);

				for (int i = 0; i < call->params.children.size(); i++)
				{
					shared_ptr<values::value> param_value;
					tie(param_value, env) = interpret(call->params.children.at(i).get(), move(env));

					auto param_name = function->parameters.at(i).name;
					function_env->v_env.set(param_name, param_value);
				}

				shared_ptr<values::value> value;
				tie(value, env) = interpret(function->body.get(), move(function_env));
				return make_tuple(value, move(env));
			}
			else if (auto exporting = dynamic_cast<core_ast::export_stmt*>(core_ast))
			{
	
			}
			else if (auto num = dynamic_cast<core_ast::integer*>(core_ast))
			{
				return make_tuple(make_shared<values::integer>(num->value), move(env));
			}
			else if (auto string = dynamic_cast<core_ast::string*>(core_ast))
			{
				return make_tuple(make_shared<values::string>(string->value), move(env));
			}

			throw runtime_error("Unknown AST node");
		}
	};
}
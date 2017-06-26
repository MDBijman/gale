#pragma once
#include "ebnfe_parser.h"
#include "language.h"
#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	struct value_environment
	{
		value_environment() {}
		value_environment(std::unordered_map<std::string, std::shared_ptr<values::value>> values) : values(values) {}
		value_environment(const value_environment& other) : values(other.values) {}

		void set(const std::string& name, std::shared_ptr<values::value> value)
		{
			values.insert_or_assign(name, value);
		}

		std::shared_ptr<values::value> get(const std::string& name)
		{
			return values.at(name);
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<values::value>> values;
	};

	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<core_ast::node>, std::shared_ptr<values::value>>
	{
	private:
		value_environment base_environment;

	public:
		interpreting_stage() {}
		interpreting_stage(value_environment base_environment) : base_environment(base_environment) {}

		std::shared_ptr<values::value> interpret(std::unique_ptr<core_ast::node> core_ast)
		{
			auto v_env = std::make_unique<value_environment>(base_environment);

			std::shared_ptr<values::value> result;
			std::tie(result, v_env) = interpret(core_ast.get(), std::move(v_env));

			return result;
		}

		std::tuple<
			std::shared_ptr<values::value>, 
			std::unique_ptr<value_environment>
		> interpret(core_ast::node* core_ast, std::unique_ptr<value_environment> v_env)
		{
			using namespace std;

			if (auto tuple_node = dynamic_cast<core_ast::tuple*>(core_ast))
			{
				shared_ptr<values::value> res;
				for (decltype(auto) line : tuple_node->children)
				{
					tie(res, v_env) = interpret(line.get(), move(v_env));
				}

				return make_tuple(move(res), move(v_env));
			}
			else if (auto id = dynamic_cast<core_ast::identifier*>(core_ast))
			{
				auto interpreted_id = v_env->get(id->name);
				return make_tuple(interpreted_id, move(v_env));
			}
			else if (auto assignment = dynamic_cast<core_ast::assignment*>(core_ast))
			{
				shared_ptr<values::value> value;
				tie(value, v_env) = move(interpret(assignment->value.get(), move(v_env)));

				v_env->set(assignment->id.name, value);

				return make_tuple(v_env->get(assignment->id.name), move(v_env));
			}
			else if (auto call = dynamic_cast<core_ast::function_call*>(core_ast))
			{
				auto function = dynamic_cast<values::function*>(v_env->get(call->id.name).get());
				auto function_v_env = make_unique<value_environment>(*v_env);
				for (int i = 0; i < call->params.children.size(); i++)
				{
					shared_ptr<values::value> param_value;
					tie(param_value, v_env) = interpret(call->params.children.at(i).get(), move(v_env));

					auto param_name = function->parameters.at(i).name;
					function_v_env->set(param_name, param_value);
				}

				shared_ptr<values::value> value;
				tie(value, v_env) = interpret(function->body.get(), move(function_v_env));
				return make_tuple(value, move(v_env));
			}
			else if (auto num = dynamic_cast<core_ast::integer*>(core_ast))
			{
				return make_tuple(make_shared<values::integer>(num->value), move(v_env));
			}
			else if (auto string = dynamic_cast<core_ast::string*>(core_ast))
			{
				return make_tuple(make_shared<values::string>(string->value), move(v_env));
			}

			throw runtime_error("Unknown AST node");
		}
	};
}
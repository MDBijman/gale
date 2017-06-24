#pragma once
#include "ebnfe_parser.h"
#include "language.h"
#include <iostream>
#include <memory>
#include <tuple>

namespace fe
{
	using namespace values;
	using namespace types;

	struct value_environment
	{
		void set(const std::string& name, std::shared_ptr<value> value)
		{
			values.insert_or_assign(name, value);
		}

		std::shared_ptr<value> get(const std::string& name)
		{
			return values.at(name);
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<value>> values;
	};

	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<core_ast::node>, std::shared_ptr<value>>
	{
	public:
		std::shared_ptr<value> interpret(std::unique_ptr<core_ast::node> core_ast)
		{
			auto interpreted_result = interpret(core_ast.get(), std::make_unique<value_environment>());
			return std::move(
				std::get<0>(std::move(interpreted_result))
			);
		}

		std::tuple<
			std::shared_ptr<value>, 
			std::unique_ptr<value_environment>
		> interpret(core_ast::node* core_ast, std::unique_ptr<value_environment>&& v_env)
		{
			if (auto tuple = dynamic_cast<core_ast::tuple*>(core_ast))
			{
				std::shared_ptr<values::tuple> res = std::make_shared<values::tuple>();
				for (decltype(auto) line : tuple->children)
				{
					auto interpreted_line = interpret(line.get(), std::move(v_env));
					v_env = std::move(std::get<1>(interpreted_line));
					res->content.push_back(std::get<0>(interpreted_line));
				}

				return std::make_tuple(std::move(res), std::move(v_env));
			}
			else if (auto id = dynamic_cast<core_ast::identifier*>(core_ast))
			{
				auto interpreted_id = v_env->get(id->name);
				return std::make_tuple(interpreted_id, std::move(v_env));
			}
			else if (auto assignment = dynamic_cast<core_ast::assignment*>(core_ast))
			{
				std::shared_ptr<value> value;
				std::tie(value, v_env) = std::move(interpret(assignment->value.get(), std::move(v_env)));

				v_env->set(assignment->id.name, value);

				return std::make_tuple(v_env->get(assignment->id.name), std::move(v_env));
			}
			else if (auto call = dynamic_cast<core_ast::function_call*>(core_ast))
			{
				if (call->id.name == "print")
				{
					auto interpreted_function = interpret(call->params.children.at(0).get(), std::move(v_env));
					v_env = std::move(std::get<1>(interpreted_function));
					std::get<0>(interpreted_function).get()->print();
					return std::make_tuple(std::make_shared<void_value>(), std::move(v_env));
				}
				else
				{
					throw std::runtime_error("unknown function");
				}
			}
			else if (auto constructor = dynamic_cast<core_ast::constructor*>(core_ast))
			{
				return interpret(&constructor->value, std::move(v_env));
			}
			else if (auto num = dynamic_cast<core_ast::integer*>(core_ast))
			{
				return std::make_tuple(std::make_shared<values::integer>(num->value), std::move(v_env));
			}
			else if (auto string = dynamic_cast<core_ast::string*>(core_ast))
			{
				return std::make_tuple(std::make_shared<values::string>(string->value), std::move(v_env));
			}

			throw std::runtime_error("Unknown AST node");
		}
	};
}
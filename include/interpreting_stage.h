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

	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<fe::ast::node>, std::shared_ptr<value>>
	{
	public:
		std::shared_ptr<value> interpret(std::unique_ptr<fe::ast::node> ast)
		{
			auto interpreted_result = interpret(ast.get(), std::make_unique<value_environment>());
			return std::move(
				std::get<0>(std::move(interpreted_result))
			);
		}

		std::tuple<
			std::shared_ptr<value>, 
			std::unique_ptr<value_environment>
		> interpret(fe::ast::node* ast, std::unique_ptr<value_environment> v_env)
		{
			if (ast::assignment* assignment = dynamic_cast<ast::assignment*>(ast))
			{
				auto interpreted_value = std::move(interpret(assignment->value.get(), std::move(v_env)));
				v_env = std::move(std::get<1>(interpreted_value));
				v_env->set(assignment->id->name, std::get<0>(interpreted_value));
				return std::make_tuple(std::make_shared<void_value>(), std::move(v_env));
			}
			else if (ast::node_list* list = dynamic_cast<ast::node_list*>(ast))
			{
				std::shared_ptr<value> res;
				for (auto& line : list->children)
				{
					auto interpreted_line = interpret(line.get(), std::move(v_env));
					v_env = std::move(std::get<1>(interpreted_line));
					res = std::get<0>(interpreted_line);
				}
				return std::make_tuple(res, std::move(v_env));
			}
			else if (ast::function_call* call = dynamic_cast<ast::function_call*>(ast))
			{
				if (call->id.name == "print")
				{
					auto interpreted_function = interpret(call->params.at(0).get(), std::move(v_env));
					v_env = std::move(std::get<1>(interpreted_function));
					std::get<0>(interpreted_function).get()->print();
					return std::make_tuple(std::make_shared<void_value>(), std::move(v_env));
				}
				else
				{
					throw std::runtime_error("unknown function");
				}
			}
			else if (ast::identifier* id = dynamic_cast<ast::identifier*>(ast))
			{
				auto interpreted_id = v_env->get(id->name);
				return std::make_tuple(interpreted_id, std::move(v_env));
			}
			else if (ast::integer* num = dynamic_cast<ast::integer*>(ast))
			{
				return std::make_tuple(std::make_shared<values::integer>(num->value), std::move(v_env));
			}
			else if (ast::string* string = dynamic_cast<ast::string*>(ast))
			{
				return std::make_tuple(std::make_shared<values::string>(string->value), std::move(v_env));
			}
			else if (ast::tuple* tuple = dynamic_cast<ast::tuple*>(ast))
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
			throw std::runtime_error("Unknown AST node");
		}
	};
}
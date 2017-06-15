#pragma once
#include "ebnfe_parser.h"
#include "language.h"
#include <iostream>
#include <memory>

namespace fe
{
	struct value
	{
		virtual void print() = 0;
	};

	struct numeric_value : public value
	{
		numeric_value(int n) : value(n) {}
		int value;

		void print() override
		{
			std::cout << value << std::endl;
		}
	};

	struct void_value : public value
	{
		void print() override
		{
			std::cout << "void" << std::endl;
		}
	};

	struct environment
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
			return std::move(interpret(ast.get(), new environment()));
		}

		std::shared_ptr<value> interpret(fe::ast::node* ast, environment* env)
		{
			if (ast::assignment* assignment = dynamic_cast<ast::assignment*>(ast))
			{
				env->set(assignment->id.name, std::move(interpret(assignment->value.get(), env)));
				return std::make_shared<void_value>();
			}
			else if (ast::node_list* list = dynamic_cast<ast::node_list*>(ast))
			{
				std::shared_ptr<value> res;
				for (auto& line : list->children) res = std::move(interpret(line.get(), env));
				return res;
			}
			else if (ast::function_call* call = dynamic_cast<ast::function_call*>(ast))
			{
				if (call->id.name == "print")
				{
					interpret(call->params.at(0).get(), env)->print();
				}
				else
				{
					throw std::runtime_error("unknown function");
				}
				return std::make_shared<void_value>();
			}
			else if (ast::identifier* id = dynamic_cast<ast::identifier*>(ast))
			{
				return env->get(id->name);
			}
			else if (ast::number* num = dynamic_cast<ast::number*>(ast))
			{
				return std::make_shared<numeric_value>(num->value);
			}
			throw std::runtime_error("Unknown AST node");
		}
	};
}
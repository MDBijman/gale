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
		void set(const std::string& name, value* value)
		{
			values.insert_or_assign(name, value);
		}

		std::unordered_map<std::string, value*> values;
	};

	class interpreting_stage : public language::interpreting_stage<std::unique_ptr<tools::ebnfe::node>, value*>
	{
	public:
		value* interpret(std::unique_ptr<tools::ebnfe::node> ast)
		{
			return interpret(ast.get(), new environment());
		}

		value* interpret(tools::ebnfe::node* ast, environment* env)
		{
			using namespace fe;
			if (std::holds_alternative<tools::ebnfe::terminal_node>(*ast))
			{
				auto terminal_node = std::get<tools::ebnfe::terminal_node>(*ast);
				auto terminal = terminal_node.value;

				if (terminal == number)
				{
					return new numeric_value{ 0 };
				}
				else if (terminal == identifier)
				{
					return env->values.at("a");
				}
				else
				{
					throw std::runtime_error("Unknown terminal");
				}
			}
			else
			{
				auto non_terminal = std::get<tools::ebnfe::non_terminal_node>(*ast).value;

				if (non_terminal == file)
				{
					for (auto& line : std::get<tools::ebnfe::non_terminal_node>(*ast).children)
					{
						interpret(line.get(), env);
					}
					return new void_value();
				}
				else if (non_terminal == print)
				{
					auto value = interpret(std::get<tools::ebnfe::non_terminal_node>(*ast).children.at(0).get(), env);
					value->print();
					return new void_value();
				}
				else if (non_terminal == assignment)
				{
					env->set("a", new numeric_value(1));
					return new void_value();
				}
				else
				{
					throw std::runtime_error("Unknown non-terminal");
				}
			}
			return nullptr;
		}
	};
}
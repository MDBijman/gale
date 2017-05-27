#pragma once
#include "ebnfe_parser.h"
#include <iostream>
namespace language
{
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


		class interpreter
		{
		public:
			value* interp(ebnfe::node* ast)
			{
				return interp(ast, new environment());
			}

			value* interp(ebnfe::node* ast, environment* env)
			{
				auto value = ast->value;
				if (value.is_terminal())
				{
					auto terminal = value.get_terminal();
					if(terminal == number)
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
					auto non_terminal = value.get_non_terminal();
					if (non_terminal == file)
					{
						for (auto line : ast->children)
						{
							interp(line, env);
						}
						return new void_value();
					}
					else if (non_terminal == print)
					{
						auto value = interp(ast->children.at(0), env);
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
				
			}
		};
	}
}
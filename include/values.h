#pragma once
#include <string>
#include <iostream>
#include "types.h"

namespace fe
{
	namespace core_ast
	{
		struct node;
		struct identifier;
	}

	namespace values
	{
		struct value
		{
			virtual ~value() {};
			virtual void print() = 0;
		};

		struct string : public value
		{
			string(std::string s) : val(s) {}
			std::string val;

			void print() override
			{
				std::cout << val;
			}
		};

		struct integer : public value
		{
			integer(int n) : val(n) {}
			int val;

			void print() override
			{
				std::cout << val;
			}
		};

		struct void_value : public value
		{
			void print() override
			{
				std::cout << "void" << std::endl;
			}
		};

		struct tuple : public value
		{
			tuple() {}
			tuple(std::vector<std::shared_ptr<value>> values) : content(values) {}
			std::vector<std::shared_ptr<value>> content;

			void print() override
			{
				std::cout << "(";

				for (auto it = content.begin(); it != content.end(); it++)
				{
					(*it)->print();
					if(it != content.end() - 1)
						std::cout << " ";
				}

				std::cout << ")";
			}
		};

		struct function : public value
		{
			function(std::vector<core_ast::identifier> params, std::unique_ptr<core_ast::node> body) : parameters(std::move(params)), body(std::move(body)) {}
			function(function&& other) : parameters(std::move(other.parameters)), body(std::move(other.body)) {}
			
			std::vector<core_ast::identifier> parameters;
			std::unique_ptr<core_ast::node> body;

			void print() override
			{
				std::cout << "function";
			}
		};

		struct native_function : public value
		{
			native_function(std::function<tuple(tuple)> f) : function(f) {}
			std::function<tuple(tuple)> function;

			void print() override
			{
				std::cout << "native function";
			}
		};

		struct module : public value
		{
			std::unordered_map<std::string, std::shared_ptr<value>> exports;

			void print() override
			{
				std::cout << "module";
			}
		};
	}
}
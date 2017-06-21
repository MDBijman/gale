#pragma once
#include <string>
#include <iostream>
#include "types.h"

namespace fe
{
	namespace values
	{
		struct value
		{
			virtual ~value() {};
			virtual void print() = 0;
			virtual types::type type() = 0;
		};

		struct string : public value
		{
			string(std::string s) : val(s) {}
			std::string val;

			void print() override
			{
				std::cout << val;
			}

			types::type type() override
			{
				return types::string_type();
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

			types::type type() override
			{
				return types::integer_type();
			}
		};

		struct void_value : public value
		{
			void print() override
			{
				std::cout << "void" << std::endl;
			}

			types::type type() override
			{
				return types::void_type();
			}
		};

		struct tuple : public value
		{
			tuple() {}
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

			types::type type() override
			{
				auto type = types::product_type();
				for (decltype(auto) value : content)
				{
					type.product.push_back(std::make_unique<types::type>(value->type()));
				}
				return type;
			}
		};
	}
}
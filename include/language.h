#pragma once
#include "ebnfe_parser.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token, lrb_token, rrb_token;
	
	tools::ebnfe::non_terminal 
		assignment, file, tuple_t, data;

	tools::ebnfe::terminal
		identifier, equals, left_bracket, right_bracket, number, word;

	namespace types
	{
		struct type
		{
			
		};

		struct sum_type : public type
		{
			std::vector<std::unique_ptr<type>> sum;
		};

		struct product_type : public type
		{
			std::vector<std::unique_ptr<type>> product;
		};

		struct integer_type : public type
		{
		};

		struct string_type : public type
		{
		};

		struct void_type : public type
		{
		};
	}

	namespace values
	{
		struct value
		{
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

	namespace ast
	{
		struct node
		{
			virtual ~node() {}
		};

		struct node_list : public node
		{
			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(const std::string& name) : name(name) {}
			std::string name;
		};

		struct type_identifier : public node
		{
			type_identifier(const std::string& name) : name(name) {}
			std::string name;
		};

		struct assignment : public node
		{
			assignment(identifier id, std::unique_ptr<node> val) : id(id), value(std::move(val)) {}
			identifier id;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier id, std::vector<std::unique_ptr<node>> params) : id(id), params(std::move(params)) {}
			identifier id;
			std::vector<std::unique_ptr<node>> params;
		};

		// Data

		struct tuple : public node
		{
			std::vector<std::unique_ptr<node>> children;
		};

		struct integer : public node
		{
			integer(values::integer val) : value(val) {}
			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : value(val) {}
			values::string value;
		};
	}

	using pipeline = language::pipeline<tools::lexing::token, tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<ast::node>, std::unique_ptr<ast::node>, std::shared_ptr<fe::values::value>>;
}

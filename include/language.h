#pragma once
#include "ebnfe_parser.h"
#include <vector>
#include <memory>

namespace fe
{
	tools::lexing::token_id
		assignment_token, word_token, number_token, lrb_token, rrb_token;
	
	tools::ebnfe::non_terminal 
		file, tuple, value;

	tools::ebnfe::terminal
		left_bracket, right_bracket, number, word;
	
	namespace values
	{
		struct value
		{
			virtual void print() = 0;
		};

		struct string : public value
		{
			string(std::string s) : value(s) {}
			std::string value;

			void print() override
			{
				std::cout << value << std::endl;
			}
		};

		struct integer : public value
		{
			integer(int n) : value(n) {}
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
	}

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

	using pipeline = language::pipeline<tools::lexing::token, tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<ast::node>, std::unique_ptr<ast::node>, std::shared_ptr<fe::value>>;
}

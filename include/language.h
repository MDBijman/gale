#pragma once
#include "ebnfe_parser.h"

namespace fe
{
	struct value;

	tools::ebnfe::non_terminal
		file, statement, assignment, print, expression, macro;

	tools::ebnfe::terminal
		equals, identifier, number, print_keyword, semicolon, lcb, rcb, macro_keyword;

	tools::lexing::token_id
		assignment_token, word_token, number_token, semicolon_token, lcb_token, rcb_token;


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

		struct number : public node
		{
			number(int val) : value(val) {}
			int value;
		};
	}


	using pipeline = language::pipeline<tools::lexing::token, tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<ast::node>, std::unique_ptr<ast::node>, std::shared_ptr<fe::value>>;
}

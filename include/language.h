#pragma once
#include <memory>

#include "ebnfe_parser.h"
#include "ast.h"

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token, lrb_token, rrb_token;
	
	tools::ebnfe::non_terminal 
		assignment, file, tuple_t, data;

	tools::ebnfe::terminal
		identifier, equals, left_bracket, right_bracket, number, word;

	using pipeline = language::pipeline<tools::lexing::token, tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<ast::node>, std::unique_ptr<ast::node>, std::shared_ptr<fe::values::value>>;
}

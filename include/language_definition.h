#pragma once
#include <memory>

#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "environment.h"

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token, lrb_token, rrb_token;
	
	tools::ebnfe::non_terminal 
		file, assignment, expression, tuple_t, export_stmt, type_definition;

	tools::ebnfe::terminal
		identifier, equals, left_bracket, right_bracket, number, word, export_keyword, type_keyword;

	using pipeline = language::pipeline<
		tools::lexing::token, 
		tools::bnf::terminal_node, 
		std::unique_ptr<tools::ebnfe::node>, 
		extended_ast::node_p,
		extended_ast::node_p,
		std::unique_ptr<core_ast::node>, 
		std::shared_ptr<values::value>,
		fe::environment
	>;
}

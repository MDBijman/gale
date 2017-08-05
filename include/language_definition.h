#pragma once
#include <memory>

#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "environment.h"

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token, lrb_token, rrb_token, module_infix_token;
	
	tools::ebnfe::non_terminal 
		file, assignment, expression, value_tuple, export_stmt, type_definition, type_tuple, statement;

	tools::ebnfe::terminal
		name, identifier, equals, left_bracket, right_bracket, number, word, export_keyword, type_keyword, module_infix;

	using pipeline = language::pipeline<
		tools::lexing::token, 
		tools::bnf::terminal_node, 
		std::unique_ptr<tools::ebnfe::node>, 
		extended_ast::node_p,
		extended_ast::node_p,
		std::unique_ptr<core_ast::node>,
		std::shared_ptr<fe::values::value>,
		fe::environment
	>;
}

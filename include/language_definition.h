#pragma once
#include <memory>

#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "environment.h"

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token, lrb_token, rrb_token, module_infix_token, plus_token, minus_token, multiply_token, divide_token;
	
	tools::ebnfe::non_terminal 
		file, assignment, expression, value_tuple, export_stmt, type_definition, type_tuple, statement, addition, subtraction, multiplication, division;

	tools::ebnfe::terminal
		identifier, equals, left_bracket, right_bracket, number, word, export_keyword, type_keyword, module_infix, plus, minus, times, divide;

	using pipeline = language::pipeline<
		tools::lexing::token, 
		tools::lexing::error,

		tools::bnf::terminal_node, 
		fe::lex_to_parse_error,

		std::unique_ptr<tools::ebnfe::node>, 
		tools::ebnfe::error,
		
		extended_ast::node,
		fe::cst_to_ast_error,

		extended_ast::node,
		fe::typecheck_error,

		core_ast::node,
		fe::lower_error,

		values::value,
		fe::interp_error,

		environment
	>;
}

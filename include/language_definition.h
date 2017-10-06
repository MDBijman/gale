#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "error.h"
#include "typecheck_environment.h"
#include "runtime_environment.h"

#include <memory>

namespace fe
{
	tools::lexing::token_id
		equals_token, keyword_token, string_token, number_token,
		lrb_token, rrb_token, right_arrow_token,
		lcb_token, rcb_token, comma_token, lsb_token, rsb_token, pipe_token;
	
	tools::ebnfe::non_terminal
		file, statement, export_stmt, assignment, expression, value_tuple, 
		tuple_element, function, branch, 
		branch_element, variable_declaration, type_expression, type_tuple, 
		type_tuple_elements, type_function, type_definition, module_declaration;

	tools::ebnfe::terminal
		identifier, equals, left_bracket, right_bracket,
		number, word, export_keyword, type_keyword, 
		function_keyword, left_curly_bracket, right_curly_bracket,
		right_arrow, comma, left_square_bracket, right_square_bracket,
		case_keyword, vertical_line, module_keyword, public_keyword,
		ref_keyword;

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

		typecheck_environment, 
		runtime_environment
	>;
}

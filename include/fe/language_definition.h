#pragma once
#include "utils/parsing/ebnfe_parser.h"

namespace fe
{
	namespace tokens
	{
		extern utils::lexing::token_id
			equals_token, keyword_token, string_token, number_token,
			lrb_token, rrb_token, right_arrow_token, semicolon_token,
			lcb_token, rcb_token, comma_token, lsb_token, rsb_token, pipe_token,
			plus_token, minus_token, mul_token, div_token, lab_token, rab_token,
			colon_token, dot_token, equality_token;
	}

	namespace non_terminals
	{
		extern utils::ebnfe::non_terminal
			file, statement, export_stmt, declaration, expression, value_tuple,
			tuple_element, function, match, operation, term, factor, addition, subtraction,
			multiplication, division, brackets, array_index, index, module_imports,
			match_branch, variable_declaration, type_expression, type_tuple,
			type_tuple_elements, function_type, type_definition, module_declaration,
			block, function_call, atom_variable_declaration, tuple_variable_declaration,
			type_atom, reference_type, array_type, reference, array_value, while_loop,
			arithmetic, equality, type_operation, type_modifiers, assignable, identifier_tuple,
			assignment
			;
	}

	namespace terminals
	{
		extern utils::ebnfe::terminal
			identifier, equals, left_bracket, right_bracket,
			number, word, export_keyword, type_keyword, function_keyword, 
			left_curly_bracket, right_curly_bracket, right_arrow, comma, 
			left_square_bracket, right_square_bracket, match_keyword, 
			vertical_line, module_keyword, public_keyword, ref_keyword, 
			var_keyword, semicolon, colon, dot, plus, minus, 
			mul, div, left_angle_bracket, right_angle_bracket,
			import_keyword, qualified_keyword, from_keyword, as_keyword,
			while_keyword, do_keyword, two_equals, on_keyword, true_keyword,
			false_keyword
			;
	}
}

#include "fe/language_definition.h"

namespace fe
{
	namespace tokens
	{
		utils::lexing::token_id
			identifier, word, number, right_arrow,
			left_bracket, right_bracket, left_angle_bracket, right_angle_bracket, 
			semicolon, left_curly_bracket, right_curly_bracket, comma, left_square_bracket, right_square_bracket, 
			vertical_line, plus, minus, mul, div, colon, dot, equals, percentage, lteq, gteq,
			fat_right_arrow, backslash, and_keyword, or_keyword, two_equals,
			type_keyword, match_keyword, module_keyword, public_keyword,
			ref_keyword, let_keyword, import_keyword, while_keyword,
			true_keyword, false_keyword, if_keyword, elseif_keyword, else_keyword
			;
	}

	namespace non_terminals
	{
		recursive_descent::non_terminal
			file, statement, export_stmt, declaration, expression, value_tuple,
			function, match, operation, term, addition, subtraction,
			multiplication, division, brackets, array_index, index, module_imports,
			match_branch, type_expression, type_tuple,
			function_type, type_definition, module_declaration,
			block, function_call, record, record_element,
			type_atom, reference_type, array_type, reference, array_value, while_loop,
			arithmetic, equality, type_operation, type_modifiers, assignable, identifier_tuple,
			assignment, greater_than, modulo, less_or_equal, comparison, greater_or_equal, less_than,
			if_expr, stmt_semicln, block_elements, block_result, elseif_expr, else_expr,
			logical, and_expr, or_expr
			;
	}
}

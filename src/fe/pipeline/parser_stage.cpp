#include "fe/pipeline/parser_stage.h"

namespace fe
{
	parsing_stage::parsing_stage()
	{
		using namespace terminals;
		using namespace non_terminals;

		{
			file = parser.new_non_terminal();
			statement = parser.new_non_terminal();
			declaration = parser.new_non_terminal();
			expression = parser.new_non_terminal();
			value_tuple = parser.new_non_terminal();
			function = parser.new_non_terminal();
			match = parser.new_non_terminal();
			match_branch = parser.new_non_terminal();
			type_expression = parser.new_non_terminal();
			type_tuple = parser.new_non_terminal();
			function_type = parser.new_non_terminal();
			type_definition = parser.new_non_terminal();
			module_declaration = parser.new_non_terminal();
			block = parser.new_non_terminal();
			function_call = parser.new_non_terminal();
			type_atom = parser.new_non_terminal();
			reference_type = parser.new_non_terminal();
			array_type = parser.new_non_terminal();
			reference = parser.new_non_terminal();
			array_value = parser.new_non_terminal();
			operation = parser.new_non_terminal();
			term = parser.new_non_terminal();
			addition = parser.new_non_terminal();
			subtraction = parser.new_non_terminal();
			division = parser.new_non_terminal();
			multiplication = parser.new_non_terminal();
			brackets = parser.new_non_terminal();
			module_imports = parser.new_non_terminal();
			while_loop = parser.new_non_terminal();
			arithmetic = parser.new_non_terminal();
			equality = parser.new_non_terminal();
			type_operation = parser.new_non_terminal();
			type_modifiers = parser.new_non_terminal();
			assignable = parser.new_non_terminal();
			identifier_tuple = parser.new_non_terminal();
			assignment = parser.new_non_terminal();
			greater_than = parser.new_non_terminal();
			modulo = parser.new_non_terminal();
			less_or_equal = parser.new_non_terminal();
			comparison = parser.new_non_terminal();
			greater_or_equal = parser.new_non_terminal();
			less_than = parser.new_non_terminal();
			if_expr = parser.new_non_terminal();
			elseif_expr = parser.new_non_terminal();
			else_expr = parser.new_non_terminal();
			stmt_semicln = parser.new_non_terminal();
			block_elements = parser.new_non_terminal();
			block_result = parser.new_non_terminal();
			record = parser.new_non_terminal();
			record_element = parser.new_non_terminal();
			logical = parser.new_non_terminal();
			and_expr = parser.new_non_terminal();
			or_expr = parser.new_non_terminal();
		}

		{
			identifier = parser.new_terminal();
			equals = parser.new_terminal();
			left_bracket = parser.new_terminal();
			right_bracket = parser.new_terminal();
			number = parser.new_terminal();
			word = parser.new_terminal();
			type_keyword = parser.new_terminal();
			left_curly_bracket = parser.new_terminal();
			right_curly_bracket = parser.new_terminal();
			right_arrow = parser.new_terminal();
			comma = parser.new_terminal();
			left_square_bracket = parser.new_terminal();
			right_square_bracket = parser.new_terminal();
			match_keyword = parser.new_terminal();
			vertical_line = parser.new_terminal();
			module_keyword = parser.new_terminal();
			public_keyword = parser.new_terminal();
			ref_keyword = parser.new_terminal();
			let_keyword = parser.new_terminal();
			semicolon = parser.new_terminal();
			plus = parser.new_terminal();
			minus = parser.new_terminal();
			mul = parser.new_terminal();
			div = parser.new_terminal();
			left_angle_bracket = parser.new_terminal();
			right_angle_bracket = parser.new_terminal();
			colon = parser.new_terminal();
			dot = parser.new_terminal();
			import_keyword = parser.new_terminal();
			while_keyword = parser.new_terminal();
			two_equals = parser.new_terminal();
			true_keyword = parser.new_terminal();
			false_keyword = parser.new_terminal();
			percentage = parser.new_terminal();
			lteq = parser.new_terminal();
			gteq = parser.new_terminal();
			if_keyword = parser.new_terminal();
			backslash = parser.new_terminal();
			fat_right_arrow = parser.new_terminal();
			else_keyword = parser.new_terminal();
			elseif_keyword = parser.new_terminal();
			and_keyword = parser.new_terminal();
			or_keyword = parser.new_terminal();
		}


		using namespace utils::ebnf::meta;
		parser
			// Initial non terminal
			.new_rule({ file, { lsb, module_declaration, rsb, lsb, module_imports, rsb, lrb, stmt_semicln, rrb, star } })
			.new_rule({ module_declaration, { module_keyword, identifier } })
			.new_rule({ module_imports, {
				import_keyword, left_square_bracket, identifier, star, right_square_bracket
			} })

			// Statements

			.new_rule({ stmt_semicln, { statement, semicolon } })

			.new_rule({ statement, {
				type_definition, alt,
				declaration, alt,
				assignment, alt,
				while_loop, alt,
				operation
			} })

			.new_rule({ type_definition, { type_keyword, identifier, equals, record } })
			.new_rule({ record, { left_bracket, record_element, lrb, comma, record_element, rrb, star, right_bracket } })
			.new_rule({ record_element, { identifier, colon, type_operation } })
			.new_rule({ declaration, { let_keyword, assignable, colon, type_operation, equals, operation } })
			.new_rule({ assignable, { identifier, alt, identifier_tuple } })
			.new_rule({ identifier_tuple, { left_bracket, identifier, comma, identifier, lrb, comma, identifier, rrb,
				star, right_bracket } })
			.new_rule({ assignment, { identifier, equals, operation } })
			.new_rule({ while_loop, { while_keyword, left_bracket, operation, right_bracket, block } })

			// Expressions/Operations
			// From least to most precedence

			.new_rule({ operation, { match } })

			.new_rule({ match, { logical, match_keyword, left_curly_bracket, match_branch, star,
				right_curly_bracket, alt, logical} })
			.new_rule({ match_branch, { vertical_line, operation, right_arrow, operation } })

			.new_rule({ logical, { and_expr, alt, or_expr, alt, comparison} })
			.new_rule({ and_expr, { comparison, and_keyword, logical } })
			.new_rule({ or_expr, { comparison, or_keyword, logical } })

			.new_rule({ comparison, { equality, alt, less_or_equal, alt, less_than, alt,
				greater_or_equal, alt, greater_than, alt, arithmetic } })
			.new_rule({ equality, { arithmetic, two_equals, comparison, } })
			.new_rule({ less_or_equal, { arithmetic, lteq, comparison } })
			.new_rule({ less_than, { arithmetic, left_angle_bracket, comparison } })
			.new_rule({ greater_or_equal, { arithmetic, gteq, comparison } })
			.new_rule({ greater_than, { arithmetic, right_angle_bracket, comparison } })

			.new_rule({ arithmetic, { addition, alt, subtraction, alt, term } })
			.new_rule({ addition, { term, plus, arithmetic } })
			.new_rule({ subtraction, { term, minus, arithmetic } })

			.new_rule({ term, { multiplication, alt, division, alt, modulo, alt, reference } })
			.new_rule({ multiplication, { reference, mul, term } })
			.new_rule({ division, { reference, div, term } })
			.new_rule({ modulo, { reference, percentage, term } })

			.new_rule({ reference, { ref_keyword, function_call, alt, function_call } })

			.new_rule({ function_call, { identifier, function_call, alt, expression } })

			.new_rule({ expression, {
				word, alt,
				value_tuple, alt,
				block, alt,
				array_value, alt,
				number, alt,
				identifier, alt,
				brackets, alt,
				true_keyword, alt,
				false_keyword, alt,
				if_expr, alt,
				function
			} })

			.new_rule({ brackets, { left_bracket, operation, right_bracket } })

			// Expression sub-rules

			.new_rule({ value_tuple, {
				left_bracket, right_bracket, alt,
				left_bracket, operation, comma, operation, lrb, comma, operation, rrb, star, right_bracket
			} })
			.new_rule({ block, { left_curly_bracket, block_elements, right_curly_bracket } })
			.new_rule({ block_elements, { statement, semicolon, block_elements, alt, lsb, block_result, rsb } })
			.new_rule({ block_result, { expression } })
			.new_rule({ array_value, { left_square_bracket, operation, lrb, comma, operation, rrb, star, right_square_bracket } })
			.new_rule({ if_expr, { if_keyword, left_bracket, operation, right_bracket, block,
				elseif_expr, star, lsb, else_expr, rsb } })
			.new_rule({ elseif_expr, { elseif_keyword, left_bracket, operation, right_bracket, block } })
			.new_rule({ else_expr, { else_keyword, block } })
			.new_rule({ function, { backslash, assignable, fat_right_arrow, expression } })

			// Type Expressions

			.new_rule({ type_operation, { type_modifiers } })

			.new_rule({ type_modifiers, { array_type, alt, reference_type, alt, function_type, alt, type_expression } })
			.new_rule({ reference_type, { ref_keyword, type_modifiers } })
			.new_rule({ array_type, { left_square_bracket, right_square_bracket, type_modifiers } })
			.new_rule({ function_type, { type_expression, right_arrow, type_modifiers } })

			.new_rule({ type_expression, {
				type_tuple, alt, type_atom
			} })

			.new_rule({ type_atom, { identifier } })
			.new_rule({ type_tuple, { left_bracket, type_operation, lrb, comma, type_operation, rrb, star, right_bracket } })
			;

		using utils::ebnfe::transformation_type;
		parser
			.new_transformation(type_operation, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(type_modifiers, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(function_type, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(greater_than, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(less_or_equal, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(assignable, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(block_elements, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(type_expression, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(match, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(two_equals, transformation_type::REMOVE)
			.new_transformation(stmt_semicln, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(equality, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(arithmetic, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(logical, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(comparison, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(reference, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(function_call, transformation_type::REPLACE_IF_ONE_CHILD)
			.new_transformation(term, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(operation, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(brackets, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(while_keyword, transformation_type::REMOVE)
			.new_transformation(percentage, transformation_type::REMOVE)
			.new_transformation(lteq, transformation_type::REMOVE)
			.new_transformation(fat_right_arrow, transformation_type::REMOVE)
			.new_transformation(gteq, transformation_type::REMOVE)
			.new_transformation(backslash, transformation_type::REMOVE)
			.new_transformation(import_keyword, transformation_type::REMOVE)
			.new_transformation(colon, transformation_type::REMOVE)
			.new_transformation(mul, transformation_type::REMOVE)
			.new_transformation(div, transformation_type::REMOVE)
			.new_transformation(minus, transformation_type::REMOVE)
			.new_transformation(plus, transformation_type::REMOVE)
			.new_transformation(semicolon, transformation_type::REMOVE)
			.new_transformation(ref_keyword, transformation_type::REMOVE)
			.new_transformation(expression, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(left_square_bracket, transformation_type::REMOVE)
			.new_transformation(right_square_bracket, transformation_type::REMOVE)
			.new_transformation(utils::ebnfe::terminal(utils::ebnf::epsilon), transformation_type::REMOVE)
			.new_transformation(statement, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(module_keyword, transformation_type::REMOVE)
			.new_transformation(left_bracket, transformation_type::REMOVE)
			.new_transformation(right_bracket, transformation_type::REMOVE)
			.new_transformation(left_curly_bracket, transformation_type::REMOVE)
			.new_transformation(right_curly_bracket, transformation_type::REMOVE)
			.new_transformation(equals, transformation_type::REMOVE)
			.new_transformation(type_keyword, transformation_type::REMOVE)
			.new_transformation(let_keyword, transformation_type::REMOVE)
			.new_transformation(match_keyword, transformation_type::REMOVE)
			.new_transformation(right_arrow, transformation_type::REMOVE)
			.new_transformation(comma, transformation_type::REMOVE)
			.new_transformation(vertical_line, transformation_type::REMOVE)
			.new_transformation(right_angle_bracket, transformation_type::REMOVE)
			.new_transformation(left_angle_bracket, transformation_type::REMOVE)
			.new_transformation(if_keyword, transformation_type::REMOVE)
			.new_transformation(elseif_keyword, transformation_type::REMOVE)
			.new_transformation(else_keyword, transformation_type::REMOVE)
			.new_transformation(elseif_expr, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(else_expr, transformation_type::REPLACE_WITH_CHILDREN)
			.new_transformation(and_keyword, transformation_type::REMOVE)
			.new_transformation(or_keyword, transformation_type::REMOVE)
			;
	}
}
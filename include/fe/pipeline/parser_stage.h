#pragma once
#include "fe/pipeline/pipeline.h"
#include "utils/parsing/ebnfe_parser.h"

#include <memory>

namespace fe
{
	class parsing_stage : public language::parsing_stage<utils::bnf::terminal_node, std::unique_ptr<utils::ebnfe::node>, utils::ebnfe::error>
	{
	public:
		parsing_stage()
		{
			using namespace terminals;
			using namespace non_terminals;

			{
				file = parser.new_non_terminal();
				statement = parser.new_non_terminal();
				export_stmt = parser.new_non_terminal();
				assignment = parser.new_non_terminal();
				expression = parser.new_non_terminal();
				value_tuple = parser.new_non_terminal();
				tuple_element = parser.new_non_terminal();
				function = parser.new_non_terminal();
				match = parser.new_non_terminal();
				match_branch = parser.new_non_terminal();
				variable_declaration = parser.new_non_terminal();
				type_expression = parser.new_non_terminal();
				type_tuple = parser.new_non_terminal();
				function_type = parser.new_non_terminal();
				type_definition = parser.new_non_terminal();
				module_declaration = parser.new_non_terminal();
				block = parser.new_non_terminal();
				function_call = parser.new_non_terminal();
				atom_variable_declaration = parser.new_non_terminal();
				tuple_variable_declaration = parser.new_non_terminal();
				type_atom = parser.new_non_terminal();
				reference_type = parser.new_non_terminal();
				array_type = parser.new_non_terminal();
				reference = parser.new_non_terminal();
				array_value = parser.new_non_terminal();
				operation = parser.new_non_terminal();
				factor = parser.new_non_terminal();
				term = parser.new_non_terminal();
				addition = parser.new_non_terminal();
				subtraction = parser.new_non_terminal();
				division = parser.new_non_terminal();
				multiplication = parser.new_non_terminal();
				brackets = parser.new_non_terminal();
				array_index = parser.new_non_terminal();
				index = parser.new_non_terminal();
				module_imports = parser.new_non_terminal();
				while_loop = parser.new_non_terminal();
				arithmetic = parser.new_non_terminal();
				equality = parser.new_non_terminal();
				type_operation = parser.new_non_terminal();
				type_modifiers = parser.new_non_terminal();
				lhs = parser.new_non_terminal();
				identifier_tuple = parser.new_non_terminal();
			}

			{
				identifier = parser.new_terminal();
				equals = parser.new_terminal();
				left_bracket = parser.new_terminal();
				right_bracket = parser.new_terminal();
				number = parser.new_terminal();
				word = parser.new_terminal();
				export_keyword = parser.new_terminal();
				type_keyword = parser.new_terminal();
				function_keyword = parser.new_terminal();
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
				var_keyword = parser.new_terminal();
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
				qualified_keyword = parser.new_terminal();
				as_keyword = parser.new_terminal();
				from_keyword = parser.new_terminal();
				while_keyword = parser.new_terminal();
				do_keyword = parser.new_terminal();
				two_equals = parser.new_terminal();
				on_keyword = parser.new_terminal();
				true_keyword = parser.new_terminal();
				false_keyword = parser.new_terminal();
			}


			using namespace utils::ebnf::meta;
			parser
				// Initial non terminal
				.new_rule({ file, { lsb, module_declaration, rsb, lsb, module_imports, rsb, lrb, statement, semicolon, rrb, star } })
				.new_rule({ module_declaration, { module_keyword, identifier } })
				.new_rule({ module_imports, {
					import_keyword, left_square_bracket, identifier, star, right_square_bracket
				} })

				// Statements

				.new_rule({ statement, {
					type_definition, alt,
					export_stmt, alt,
					assignment, alt,
					while_loop, alt,
					operation
				} })

				.new_rule({ type_definition, { type_keyword, identifier, equals, variable_declaration } })
				.new_rule({ export_stmt, { export_keyword, identifier, star } })
				.new_rule({ assignment, { var_keyword, lhs, equals, operation } })
				.new_rule({ lhs, { identifier, alt, identifier_tuple } })
				.new_rule({ identifier_tuple, { left_bracket, identifier, comma, identifier, lrb, comma, identifier, rrb, star, right_bracket } })
				.new_rule({ while_loop, { while_keyword, operation, do_keyword, operation } })

				// Expressions/Operations
				// From least to most precedence

				.new_rule({ operation, { function, alt, match } })
				.new_rule({ function, { function_keyword, variable_declaration, right_arrow, type_operation, equals, match } })

				.new_rule({ match, { equality, match_keyword, left_curly_bracket, match_branch, star, right_curly_bracket, alt, equality } })
				.new_rule({ match_branch, { vertical_line, operation, right_arrow, operation } })

				.new_rule({ equality, { arithmetic, two_equals, equality, alt, arithmetic } })

				.new_rule({ arithmetic, { addition, alt, subtraction, alt, term } })
				.new_rule({ addition, { term, plus, arithmetic } })
				.new_rule({ subtraction, { term, minus, arithmetic } })

				.new_rule({ term, { multiplication, alt, division, alt, array_index } })
				.new_rule({ multiplication, { array_index, mul, term } })
				.new_rule({ division, { array_index, div, term } })

				.new_rule({ array_index, { index, alt, reference } })
				.new_rule({ index, { reference, colon, array_index } })

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
					false_keyword
				} })

				.new_rule({ brackets, { left_bracket, operation, right_bracket } })

				// Expression sub-rules

				.new_rule({ value_tuple, {
					left_bracket, right_bracket, alt,
					left_bracket, operation, comma, operation, lrb, comma, operation, rrb, star, right_bracket
				} })
				.new_rule({ block, { left_curly_bracket, lrb, statement, semicolon, rrb, star, right_curly_bracket } })
				.new_rule({ array_value, { left_square_bracket, operation, lrb, comma, operation, rrb, star, right_square_bracket } })

				// Declarations

				.new_rule({ variable_declaration, {
					tuple_variable_declaration, alt,
					atom_variable_declaration
				} })

				.new_rule({ atom_variable_declaration, { type_operation, identifier } })
				.new_rule({ tuple_variable_declaration, {
					left_bracket, right_bracket, alt,
					left_bracket, variable_declaration, comma, variable_declaration, lrb, comma, variable_declaration, rrb, star, right_bracket
				} })

				// Type Expressions

				.new_rule({ type_operation, { type_modifiers } })

				.new_rule({ type_modifiers, { array_type, alt, reference_type, alt, function_type } })
				.new_rule({ reference_type, { ref_keyword, type_modifiers } })
				.new_rule({ array_type, { left_square_bracket, right_square_bracket, type_modifiers } })

				.new_rule({ function_type, { type_expression, right_arrow, type_expression, alt, type_expression } })

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
				.new_transformation(lhs, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(type_expression, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(match, transformation_type::REPLACE_IF_ONE_CHILD)
				.new_transformation(two_equals, transformation_type::REMOVE)
				.new_transformation(on_keyword, transformation_type::REMOVE)
				.new_transformation(equality, transformation_type::REPLACE_IF_ONE_CHILD)
				.new_transformation(arithmetic, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(reference, transformation_type::REPLACE_IF_ONE_CHILD)
				.new_transformation(function_call, transformation_type::REPLACE_IF_ONE_CHILD)
				.new_transformation(array_index, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(term, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(factor, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(operation, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(brackets, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(while_keyword, transformation_type::REMOVE)
				.new_transformation(do_keyword, transformation_type::REMOVE)
				.new_transformation(import_keyword, transformation_type::REMOVE)
				.new_transformation(as_keyword, transformation_type::REMOVE)
				.new_transformation(from_keyword, transformation_type::REMOVE)
				.new_transformation(qualified_keyword, transformation_type::REMOVE)
				.new_transformation(colon, transformation_type::REMOVE)
				.new_transformation(mul, transformation_type::REMOVE)
				.new_transformation(div, transformation_type::REMOVE)
				.new_transformation(minus, transformation_type::REMOVE)
				.new_transformation(plus, transformation_type::REMOVE)
				.new_transformation(semicolon, transformation_type::REMOVE)
				.new_transformation(ref_keyword, transformation_type::REMOVE)
				.new_transformation(expression, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(variable_declaration, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(left_square_bracket, transformation_type::REMOVE)
				.new_transformation(right_square_bracket, transformation_type::REMOVE)
				.new_transformation(utils::ebnfe::terminal(utils::ebnf::epsilon), transformation_type::REMOVE)
				.new_transformation(tuple_element, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(type_tuple_elements, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(statement, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(module_keyword, transformation_type::REMOVE)
				.new_transformation(left_bracket, transformation_type::REMOVE)
				.new_transformation(right_bracket, transformation_type::REMOVE)
				.new_transformation(left_curly_bracket, transformation_type::REMOVE)
				.new_transformation(right_curly_bracket, transformation_type::REMOVE)
				.new_transformation(equals, transformation_type::REMOVE)
				.new_transformation(export_keyword, transformation_type::REMOVE)
				.new_transformation(type_keyword, transformation_type::REMOVE)
				.new_transformation(function_keyword, transformation_type::REMOVE)
				.new_transformation(var_keyword, transformation_type::REMOVE)
				.new_transformation(match_keyword, transformation_type::REMOVE)
				.new_transformation(right_arrow, transformation_type::REMOVE)
				.new_transformation(comma, transformation_type::REMOVE)
				.new_transformation(vertical_line, transformation_type::REMOVE)
				;
		}

		std::variant<std::unique_ptr<utils::ebnfe::node>, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) override
		{
			return parser.parse(fe::non_terminals::file, in);
		}

	private:
		utils::ebnfe::parser parser;
	};
}
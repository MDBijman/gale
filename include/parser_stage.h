#pragma once
#include "pipeline.h"
#include "ebnfe_parser.h"
#include <memory>

namespace fe
{
	class parsing_stage : public language::parsing_stage<tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>, tools::ebnfe::error>
	{
	public:
		parsing_stage()
		{
			using namespace terminals;
			using namespace non_terminals;

			file = parser.new_non_terminal();
			statement = parser.new_non_terminal();
			export_stmt = parser.new_non_terminal();
			assignment = parser.new_non_terminal();
			expression = parser.new_non_terminal();
			value_tuple = parser.new_non_terminal();
			tuple_element = parser.new_non_terminal();
			function = parser.new_non_terminal();
			branch = parser.new_non_terminal();
			branch_element = parser.new_non_terminal();
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
			case_keyword = parser.new_terminal();
			vertical_line = parser.new_terminal();
			module_keyword = parser.new_terminal();
			public_keyword = parser.new_terminal();
			ref_keyword = parser.new_terminal();
			call_keyword = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				// Initial non terminal
				.new_rule({ file, { lsb, module_declaration, rsb, statement, star } })
				.new_rule({ module_declaration, { module_keyword, identifier } })
				
				// Statements

				.new_rule({ statement, { 
					type_definition, alt, 
					export_stmt, alt, 
					assignment 
				} })

				.new_rule({ type_definition, { type_keyword, identifier, variable_declaration } })
				.new_rule({ export_stmt, { export_keyword, identifier, star } })
				.new_rule({ assignment, { identifier, equals, expression } })

				// Expressions

				.new_rule({ expression, {
					// Terminals
					number, alt,
					word, alt,
					identifier, alt,
					// Non terminals
					value_tuple, alt,
					function_call, alt,
					function, alt,
					branch, alt,
					block, alt,
					reference
				} })

				.new_rule({ value_tuple, { left_bracket, lsb, expression, lrb, comma, expression, rrb, star, rsb, right_bracket } })
				.new_rule({ function_call, { call_keyword, identifier, expression } })
				.new_rule({ function, { function_keyword, variable_declaration, right_arrow, type_expression, equals, expression} })
				.new_rule({ branch, { case_keyword, left_square_bracket, branch_element, star, right_square_bracket } })
				.new_rule({ branch_element, { vertical_line, expression, right_arrow, expression } })
				.new_rule({ block, { left_curly_bracket, expression, star, right_curly_bracket } })
				.new_rule({ reference, { ref_keyword, expression } })

				// Declarations

				.new_rule({ variable_declaration, {
					tuple_variable_declaration, alt,
					atom_variable_declaration
				} })

				.new_rule({ atom_variable_declaration, { type_expression, identifier } })
				.new_rule({ tuple_variable_declaration, { left_bracket, variable_declaration, lrb, comma, variable_declaration, rrb, star, right_bracket } })

				// Type Expressions

				.new_rule({ type_expression, {
					reference_type, alt,
					array_type, alt,
					function_type, alt,
					type_tuple, alt,
					identifier
				} })

				.new_rule({ type_tuple, { left_bracket, type_expression, lrb, comma, type_expression, rrb, star, right_bracket } })
				.new_rule({ function_type, { function_keyword, type_expression, right_arrow, type_expression } })
				.new_rule({ reference_type, { ref_keyword, type_expression } })
				.new_rule({ array_type, { left_square_bracket, right_square_bracket, type_expression } })
				;

			using tools::ebnfe::transformation_type;
			parser
				.new_transformation(ref_keyword, transformation_type::REMOVE)
				.new_transformation(expression, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(variable_declaration, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(type_expression, transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(left_square_bracket, transformation_type::REMOVE)
				.new_transformation(right_square_bracket, transformation_type::REMOVE)
				.new_transformation(tools::ebnfe::terminal(tools::ebnf::epsilon), transformation_type::REMOVE)
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
				.new_transformation(call_keyword, transformation_type::REMOVE)
				.new_transformation(case_keyword, transformation_type::REMOVE)
				.new_transformation(right_arrow, transformation_type::REMOVE)
				.new_transformation(comma, transformation_type::REMOVE)
				.new_transformation(vertical_line, transformation_type::REMOVE)
				;
		}

		std::variant<std::unique_ptr<tools::ebnfe::node>, tools::ebnfe::error> parse(const std::vector<tools::bnf::terminal_node>& in) override
		{
			return parser.parse(fe::non_terminals::file, in);
		}

	private:
		tools::ebnfe::parser parser;
	};
}
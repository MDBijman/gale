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
			type_tuple_elements = parser.new_non_terminal();
			type_function = parser.new_non_terminal();
			type_definition = parser.new_non_terminal();
			module_declaration = parser.new_non_terminal();

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
			branch_keyword = parser.new_terminal();
			vertical_line = parser.new_terminal();
			module_keyword = parser.new_terminal();
			public_keyword = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				// Starter non terminal
				.new_rule({ file, { lsb, module_declaration, rsb, statement, star } })
				.new_rule({ module_declaration, { module_keyword, identifier } })
				.new_rule({ statement, { type_definition, alt, export_stmt, alt, assignment } })
				.new_rule({ export_stmt, { export_keyword, identifier, star } })
				.new_rule({ assignment, { identifier, equals, expression } })
				.new_rule({ expression, {
					value_tuple, alt,
					word, alt,
					identifier, lsb, expression, rsb, alt,
					number, alt,
					function, alt,
					branch, alt,
					left_curly_bracket, expression, star, right_curly_bracket
				} })
				.new_rule({ value_tuple, { left_bracket, lsb, tuple_element, rsb, right_bracket } })
				.new_rule({ tuple_element, { expression, lsb, comma, tuple_element, rsb } })
				.new_rule({ function, { function_keyword, variable_declaration, right_arrow, type_expression, equals, expression} })

				.new_rule({ branch, { branch_keyword, left_square_bracket, branch_element, star, right_square_bracket } })
				.new_rule({ branch_element, { vertical_line, expression, right_arrow, expression } })

				.new_rule({ variable_declaration, {
					left_bracket, type_expression, identifier, lrb, comma, type_expression, identifier, rrb, star, right_bracket, alt,
					identifier, identifier
				} })

				// Type system
				.new_rule({ type_expression, {
					identifier, alt,
					type_function, alt,
					type_tuple
				} })
				.new_rule({ type_tuple, { left_bracket, type_tuple_elements, right_bracket } })
				.new_rule({ type_tuple_elements, { type_expression, lsb, comma, type_tuple_elements, rsb } })
				
				.new_rule({ type_function, { function_keyword, type_expression, right_arrow, type_expression } })

				.new_rule({ type_definition, { type_keyword, identifier, variable_declaration } })
				;

			parser
				.new_transformation(tools::ebnfe::terminal(tools::ebnf::epsilon), tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(tuple_element, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(type_tuple_elements, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(statement, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(module_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(left_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(left_square_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_square_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(left_curly_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_curly_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(equals, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(export_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(type_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(function_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(branch_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_arrow, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(comma, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(vertical_line, tools::ebnfe::transformation_type::REMOVE)
				;
		}

		std::variant<std::unique_ptr<tools::ebnfe::node>, tools::ebnfe::error> parse(const std::vector<tools::bnf::terminal_node>& in) override
		{
			return parser.parse(fe::file, in);
		}

	private:
		tools::ebnfe::parser parser;
	};
}
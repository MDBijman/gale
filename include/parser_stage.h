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
			assignment = parser.new_non_terminal();
			expression = parser.new_non_terminal();
			value_tuple = parser.new_non_terminal();
			type_definition = parser.new_non_terminal();
			statement = parser.new_non_terminal();
			export_stmt = parser.new_non_terminal();
			type_tuple = parser.new_non_terminal();
			addition = parser.new_non_terminal();
			subtraction = parser.new_non_terminal();
			multiplication = parser.new_non_terminal();
			division = parser.new_non_terminal();

			plus = parser.new_terminal();
			minus = parser.new_terminal();
			times = parser.new_terminal();
			divide = parser.new_terminal();
			identifier = parser.new_terminal();
			module_infix = parser.new_terminal();
			export_keyword = parser.new_terminal();
			type_keyword = parser.new_terminal();
			left_bracket = parser.new_terminal();
			right_bracket = parser.new_terminal();
			equals = parser.new_terminal();
			number = parser.new_terminal();
			word = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				.new_rule({ file, { statement, star } })
				.new_rule({ statement, { type_definition, alt, export_stmt, alt, assignment } })
				.new_rule({ export_stmt, { export_keyword, identifier } })
				.new_rule({ assignment, { identifier, equals, expression } })
				.new_rule({ expression, { value_tuple, alt, number, alt, word, alt, identifier, lsb, value_tuple, rsb, alt, addition, alt, subtraction, alt, multiplication, alt, division } })
				.new_rule({ value_tuple, { left_bracket, expression, star, right_bracket } })
				.new_rule({ addition, { plus, expression, expression } })
				.new_rule({ subtraction, { minus, expression, expression } })
				.new_rule({ multiplication, { times, expression, expression } })
				.new_rule({ division, { divide, expression, expression } })

				// Type system
				.new_rule({ type_definition, { type_keyword, identifier, type_tuple } })
				.new_rule({ type_tuple, { left_bracket, identifier, star, right_bracket } })
			;

			parser
				.new_transformation(statement, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(left_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(equals, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(export_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(type_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(plus, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(minus, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(times, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(divide, tools::ebnfe::transformation_type::REMOVE)
			;
		}

		std::variant<std::unique_ptr<tools::ebnfe::node>, tools::ebnfe::error> parse(const std::vector<tools::bnf::terminal_node>& in) override
		{
			auto cst = parser.parse(fe::file, in);

			if (std::holds_alternative<tools::ebnfe::error>(cst))
			{
				return std::get<tools::ebnfe::error>(cst);
			}
			else
			{
				return std::get<std::unique_ptr<tools::ebnfe::node>>(std::move(cst));
			}
		}

	private:
		tools::ebnfe::parser parser;
	};
}
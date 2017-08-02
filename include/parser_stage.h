#pragma once
#include "pipeline.h"
#include "ebnfe_parser.h"
#include <memory>

namespace fe
{
	class parsing_stage : public language::parsing_stage<tools::bnf::terminal_node, std::unique_ptr<tools::ebnfe::node>>
	{
	public:
		parsing_stage()
		{
			file = parser.new_non_terminal();
			assignment = parser.new_non_terminal();
			expression = parser.new_non_terminal();
			tuple_t = parser.new_non_terminal();
			
			type_definition = parser.new_non_terminal();
			statement = parser.new_non_terminal();
			export_stmt = parser.new_non_terminal();
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
				.new_rule({ type_definition, { type_keyword, identifier, tuple_t } })
				.new_rule({ export_stmt, { export_keyword, identifier } })
				.new_rule({ assignment, { identifier, equals, expression } })
				.new_rule({ expression, { tuple_t, alt, number, alt, word, alt, identifier, lsb, tuple_t, rsb } })
				.new_rule({ tuple_t, { left_bracket, expression, star, right_bracket } })
			;

			parser
				.new_transformation(statement, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(left_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(equals, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(export_keyword, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(type_keyword, tools::ebnfe::transformation_type::REMOVE)
			;
		}

		std::unique_ptr<tools::ebnfe::node> parse(const std::vector<tools::bnf::terminal_node>& in) override
		{
			auto cst = parser.parse(fe::file, in);
			return std::get<0>(std::move(cst));
		}

	private:
		tools::ebnfe::parser parser;
	};
}
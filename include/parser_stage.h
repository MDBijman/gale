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
			using namespace fe;
			file = parser.new_non_terminal();
			tuple = parser.new_non_terminal();
			value = parser.new_non_terminal();

			right_bracket = parser.new_terminal();
			left_bracket = parser.new_terminal();
			number = parser.new_terminal();
			word = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				.new_rule({ file, { tuple } })
				.new_rule({ tuple, { left_bracket, value, star, right_bracket });

			parser
				.new_transformation(semicolon, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(statement, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
				.new_transformation(equals, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(print_keyword, tools::ebnfe::transformation_type::REMOVE);
		}

		std::unique_ptr<tools::ebnfe::node> parse(const std::vector<tools::bnf::terminal_node>& in)
		{
			auto cst_or_error = parser.parse(fe::file, in);
			return std::move(std::get<std::unique_ptr<tools::ebnfe::node>>(cst_or_error));
		}

	private:
		tools::ebnfe::parser parser;
	};
}
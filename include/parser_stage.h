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
			statement = parser.new_non_terminal();
			assignment = parser.new_non_terminal();
			print = parser.new_non_terminal();

			equals = parser.new_terminal();
			identifier = parser.new_terminal();
			number = parser.new_terminal();
			print_keyword = parser.new_terminal();
			semicolon = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				.new_rule({ assignment, { identifier, equals, number } })
				.new_rule({ print, { print_keyword, identifier } })
				.new_rule({ statement, { assignment, semicolon, alt, print, semicolon } })
				.new_rule({ file, { statement, star } });

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
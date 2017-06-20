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
			tuple_t = parser.new_non_terminal();
			data = parser.new_non_terminal();
			assignment = parser.new_non_terminal();

			right_bracket = parser.new_terminal();
			left_bracket = parser.new_terminal();
			number = parser.new_terminal();
			word = parser.new_terminal();
			equals = parser.new_terminal();
			identifier = parser.new_terminal();
			type_identifier = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser
				.new_rule({ file,  { assignment } })
				.new_rule({ assignment, { identifier, equals, type_identifier, tuple_t } })
				.new_rule({ data, { tuple_t, alt, number, alt, word } })
				.new_rule({ tuple_t, { left_bracket, data, star, right_bracket } });

			parser
				.new_transformation(left_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(right_bracket, tools::ebnfe::transformation_type::REMOVE)
				.new_transformation(equals, tools::ebnfe::transformation_type::REMOVE);
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
#pragma once
#include "utils/parsing/ebnfe_parser.h"

#include <catch2/catch.hpp>
#include <vector>

SCENARIO("an ebnfe parser should parse correctly given a set of rules", "[ebnfe_parser]")
{
	GIVEN("an ebnfe parser with a set of rules")
	{
		tools::ebnfe::parser parser;

		parser = tools::ebnfe::parser{};

		tools::ebnfe::non_terminal Expression, Atom;
		Expression = parser.new_non_terminal();
		Atom = parser.new_non_terminal();

		tools::ebnfe::terminal id, plus, number;
		id = parser.new_terminal();
		plus = parser.new_terminal();
		number = parser.new_terminal();

		using namespace tools::ebnf::meta;
		parser.new_rule({ Expression, { 
			Atom, plus, Expression, alt, Atom
		} });
		parser.new_rule({ Atom, { number, alt, id } });
		parser.new_transformation(Atom, tools::ebnfe::transformation_type::REPLACE_WITH_CHILDREN);
		parser.new_transformation(Expression, tools::ebnfe::transformation_type::REPLACE_IF_ONE_CHILD);

			using namespace tools;
		WHEN("an id as a value is parsed")
		{
			auto output_or_error = parser.parse(Expression, std::vector<tools::bnf::terminal_node>{
				{ id, "a" }, { plus, "+" }, { number, "5" }
			});

			auto& output = std::get<std::unique_ptr<tools::ebnfe::node>>(output_or_error);

			THEN("the resulting parse tree should be correct")
			{
				// Expression
				auto& root = std::get<tools::ebnfe::non_terminal_node>(*output);
				REQUIRE(root.value == Expression);

				// id
				{
					auto& i = std::get<ebnfe::terminal_node>(*root.children.at(0).get());
					REQUIRE(i.value == id);
					REQUIRE(i.token == "a");
				}

				// plus
				{
					auto& p = std::get<ebnfe::terminal_node>(*root.children.at(1).get());
					REQUIRE(p.value == plus);
					REQUIRE(p.token == "+");
				}

				// number
				{
					auto& num = std::get<ebnfe::terminal_node>(*root.children.at(2).get());
					REQUIRE(num.value == number);
					REQUIRE(num.token == "5");
				}
			}
		}

		WHEN("a single id is parsed")
		{
			auto output_or_error = parser.parse(Expression, std::vector<tools::bnf::terminal_node>{
				{ id, "a" }
			});

			auto& output = std::get<std::unique_ptr<tools::ebnfe::node>>(output_or_error);

			THEN("the resulting parse tree should be correct")
			{
				// Expression
				auto& root = std::get<tools::ebnfe::non_terminal_node>(*output);
				REQUIRE(root.value == Expression);

				// id 
				{
					auto& i = std::get<ebnfe::terminal_node>(*root.children.at(0).get());
					REQUIRE(i.value == id);
					REQUIRE(i.token == "a");
				}
			}
		}
	}
}

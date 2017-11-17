#pragma once
#include "utils/parsing/lr_parser.h"
#include <catch2/catch.hpp>

SCENARIO("an lr parser should generate a correct parse table", "[lr_parser]")
{
	GIVEN("an lr parser with a set of non-conflicting rules")
	{
		using namespace tools::bnf;

		auto p = tools::lr::parser();


		non_terminal Expression = 1, Atom = 2;
		terminal id = 1, plus = 2, number = 3;

		std::multimap<non_terminal, std::vector<symbol>> rules;
		rules.insert({ Expression, { Atom, plus, Expression } });
		rules.insert({ Expression, { Atom } });
		rules.insert({ Atom, { number } });
		rules.insert({ Atom, { id } });

		WHEN("it's table is generated")
		{
			p.generate(Expression, rules);

			THEN("it shouldn't report any conflicts")
			{
				SUCCEED();
			}
		}
	}
}

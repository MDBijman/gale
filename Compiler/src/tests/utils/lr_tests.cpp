#include <catch2/catch.hpp>
#include "utils/parsing/lr_parser.h"

TEST_CASE("an lr parser should generate a correct parse table", "[lr_parser]")
{
	using namespace utils::bnf;
	auto p = utils::lr::parser();

	SECTION("an lr parser with a set of non-conflicting rules")
	{
		non_terminal Expression = 1, Atom = 2;
		terminal id = 1, plus = 2, number = 3;

		std::multimap<non_terminal, std::vector<symbol>> rules;
		rules.insert({ Expression, { Atom, plus, Expression } });
		rules.insert({ Expression, { Atom } });
		rules.insert({ Atom, { number } });
		rules.insert({ Atom, { id } });

		try
		{
			p.generate(Expression, rules);
		}
		catch (...)
		{
			FAIL();
		}
		SUCCEED();
	}
}

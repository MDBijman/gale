#pragma once
#include "lalr_parser.h"

namespace tests::lalr
{
	struct lalr_generator_tests
	{
		void run_all()
		{
			using namespace tools::bnf;
			auto parser = tools::lalr::parser();
			parser.start_symbol = 0;
			parser.new_rule(rule(0, { non_terminal(1) }));
			parser.new_rule(rule(1, { terminal(2) }));
			parser.generate_item_sets();
			assert(parser.item_sets.size() == 1);
			assert(parser.item_sets.at(0).rules.size() == 2);
		}
	};
}
#pragma once
#include "lalr_parser.h"

namespace tests::lalr
{
	struct lalr_generator_tests
	{
		void run_all()
		{
			using namespace tools::bnf;

			auto p = tools::lalr::parser();

			auto S = non_terminal(0), N = non_terminal(1), V = non_terminal(2), E = non_terminal(3);
			int eq = 4, x = 5, mul = 6;

			p.start_symbol = 0;
			p.new_rule(rule(S, { N }));
			p.new_rule(rule(N, { V, eq, E }));
			p.new_rule(rule(N, { E }));
			p.new_rule(rule(E, { V }));
			p.new_rule(rule(V, { x }));
			p.new_rule(rule(V, { mul, E }));
			p.generate_item_sets();
			p.parse({ { x, eq, mul, x } });

			assert(p.item_sets.size() == 10);
			assert(p.transitions.size() == 14);

			//auto Sc = non_terminal(0), S = non_terminal(1), A = non_terminal(2), E = non_terminal(3);
			//int eoi = 4, semicolon = 5, id = 6, set = 7, plus = 8;

			//p.start_symbol = Sc;
			///*0*/p.new_rule(rule{ Sc, {S} });
			///*1*/p.new_rule(rule{ S, {S, semicolon, A} });
			///*2*/p.new_rule(rule{ S, {A} });
			///*3*/p.new_rule(rule{ A, {E} });
			///*4*/p.new_rule(rule{ A, {id, set, E} });
			///*5*/p.new_rule(rule{ E, {E, plus, id} });
			///*6*/p.new_rule(rule{ E, {id} });
			//p.generate_item_sets();
			//p.parse({ { id, semicolon, id } });
		}
	};
}

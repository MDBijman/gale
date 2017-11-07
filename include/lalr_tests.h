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

			non_terminal E = 1, O = 2, V = 3, T = 4, F = 5;
			int n = 1, lb = 2, rb = 3, mul = 4, plus = 5, comma = 6, minus = 7, div = 8;

			std::multimap<non_terminal, std::vector<symbol>> rules;
			rules.insert({ E, { F } });
			rules.insert({ E, { V } });

			rules.insert({ V, { lb, rb } });
			rules.insert({ V, { lb, E, E, rb } });

			rules.insert({ O, { T, plus, E } });
			rules.insert({ O, { T } });

			rules.insert({ T, { F, mul, T } });
			rules.insert({ T, { F } });

			rules.insert({ F, { lb, n, rb } });
			rules.insert({ F, { n } });


			p.generate(E, rules);
			auto res = p.parse({ {n, ""} });

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

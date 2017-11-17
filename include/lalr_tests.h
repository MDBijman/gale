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

			non_terminal File = 1, Expression = 4,
				ValueTuple = 5, Factor = 3, Brackets = 2, ValueTuple2 = 6;
			int semicolon = 1, id = 3, equals = 4, lb = 5, rb = 2, comma = 6;

			std::multimap<non_terminal, std::vector<symbol>> rules;
			rules.insert({ File, { Expression, semicolon } });
			rules.insert({ Expression, { Factor } });
			rules.insert({ Expression, { ValueTuple } });
			//rules.insert({ ValueTuple, { lb, Expression, comma, rb } });
			rules.insert({ ValueTuple, { lb, Factor, ValueTuple2, rb } });
			rules.insert({ ValueTuple2, { tools::bnf::epsilon } });
			rules.insert({ ValueTuple2, { comma, Expression, ValueTuple2  } });

			rules.insert({ Factor, { lb, Factor, rb} });
			rules.insert({ Factor, { id } });

			p.generate(File, rules);
			auto res = p.parse({ });

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

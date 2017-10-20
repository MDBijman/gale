#pragma once
#include "bnf_grammar.h"

namespace tools::lalr
{
	struct lalr_rhs
	{
		std::vector<bnf::symbol> symbols;
		std::size_t bullet_offset;
	};

	struct item_set
	{
	public:
		std::unordered_map<bnf::non_terminal, lalr_rhs> rules;
	};

	class parser
	{
	public:
		void generate_item_sets();

		parser& new_rule(bnf::rule& r)
		{
			rules.insert({ r.lhs, r.rhs });
			return *this;
		}

		bnf::non_terminal start_symbol;
		std::unordered_map<bnf::non_terminal, std::vector<bnf::symbol>> rules;
		std::vector<item_set> item_sets;
	};
}

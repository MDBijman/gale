#pragma once
#include "bnf_grammar.h"

namespace utils
{
	class parser
	{
	public:
		virtual void generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& rules) = 0;

		virtual bnf::tree parse(std::vector<bnf::terminal_node>) = 0;
	};
}
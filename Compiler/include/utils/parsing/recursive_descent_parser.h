#pragma once
#include "bnf_grammar.h"

namespace recursive_descent
{
	using tree = utils::bnf::tree;

	struct error
	{
		std::string message;
	};

	void generate();
	std::variant<tree, error> parse(const std::vector<utils::bnf::terminal_node>& in);
}
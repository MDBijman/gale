#pragma once
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <string>
#include <variant>
#include <iostream>
#include "ebnf.h"
#include "ast.h"

namespace ebnf
{
	class parser
	{
	public:
		parser(rules mapping) : mapping(mapping) {}

		ast::node<symbol>* parse(std::vector<terminal> input);

	private:
		rules mapping;
		std::stack<ast::node<symbol>*> stack;
	};
}
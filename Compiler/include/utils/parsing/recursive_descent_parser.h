#pragma once
#include "fe/data/ext_ast.h"
#include "utils/lexing/lexer.h"

namespace recursive_descent
{
	using tree = fe::ext_ast::ast;
	using non_terminal = uint64_t;

	struct error
	{
		std::string message;
	};

	void generate();
	std::variant<tree, error> parse(const std::vector<utils::lexing::token>& in);
}
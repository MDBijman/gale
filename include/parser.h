#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <string_view>
#include "lexer.h"
#include "ast.h"

namespace parser
{
	using token = std::string;

	class parser
	{
	public:
		parser(lexer::file_lexer& lexer);
		ast::node parse();
	};
}

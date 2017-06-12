#pragma once
#include "ebnfe_parser.h"

namespace fe
{
	struct value;

	tools::ebnfe::non_terminal
		file, statement, assignment, print;

	tools::ebnfe::terminal
		equals, identifier, number, print_keyword, semicolon;

	tools::lexing::token_id
		assignment_token, word_token, number_token, semicolon_token;

	using pipeline = language::pipeline<tools::lexing::token, tools::ebnfe::terminal, std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<tools::ebnfe::node>, fe::value*>;
}

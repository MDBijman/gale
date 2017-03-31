#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "lexer_generator.h"
#include "lexer.h"
#include "parser.h"

int main()
{
	auto lexer = lexer::generator::generate("./snippets/lexer_specification.txt");
	auto test_lexer = lexer.lex_file("./snippets/test.fe");

	auto parser = parser::parser(test_lexer);
	auto ast_root = parser.parse();

	std::cin.get();
}
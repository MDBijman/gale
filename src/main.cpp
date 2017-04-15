#include <stdio.h>
#include <iostream>
#include "lexer_generator.h"

int main()
{
	auto lexer = lexer_generator::generate("./snippets/lexer_specification.txt");

	std::cin.get();
}
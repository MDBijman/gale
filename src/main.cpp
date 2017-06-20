#include "pipeline.h"
#include "ebnfe_parser.h"

#include "lexer_stage.h"
#include "lexer_to_parser_stage.h"
#include "parser_stage.h"
#include "cst_to_ast_stage.h"
#include "lowering_stage.h"
#include "interpreting_stage.h"

#include <stdio.h>
#include <functional>
#include <algorithm>
#include <fstream>
#include <iostream>

int main()
{
	// Parse rules
	std::ifstream in("./snippets/testing.fe", std::ios::in | std::ios::binary);
	if (!in) throw std::exception("Could not open file");

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();

	fe::pipeline p;

	{
		auto lexing_stage = new fe::lexing_stage{};
		auto parsing_stage = new fe::parsing_stage{};
		// LtP stage initialization has a dependency on the values of the language terminals
		// These are initialized in the parsing stage initialization
		auto lexer_to_parser_stage = new fe::lexer_to_parser_stage{};
		auto parser_to_lowerer_stage = new fe::cst_to_ast_stage{};
		auto lowering_stage = new fe::lowering_stage{};
		auto interpreting_stage = new fe::interpreting_stage{};

		p
			.lexer(lexing_stage)
			.lexer_to_parser(lexer_to_parser_stage)
			.parser(std::move(parsing_stage))
			.parser_to_lowerer(parser_to_lowerer_stage)
			.lowerer(lowering_stage)
			.interpreter(interpreting_stage);
	}


	std::shared_ptr<fe::value> result = p.process(std::move(contents));
	result->print();

	std::cin.get();
}
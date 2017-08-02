#include "pipeline.h"
#include "ebnfe_parser.h"

#include "lexer_stage.h"
#include "lexer_to_parser_stage.h"
#include "parser_stage.h"
#include "cst_to_ast_stage.h"
#include "typechecker_stage.h"
#include "lowering_stage.h"
#include "interpreting_stage.h"

#include "values.h"

#include "language_module.h"

#include <stdio.h>
#include <functional>
#include <algorithm>
#include <fstream>
#include <iostream>

fe::pipeline create_pipeline()
{
	fe::pipeline p;

	auto lexing_stage = new fe::lexing_stage{};
	auto parsing_stage = new fe::parsing_stage{};
	// LtP stage initialization has a dependency on the values of the language terminals
	// These are initialized in the parsing stage initialization
	auto lexer_to_parser_stage = new fe::lexer_to_parser_stage{};
	auto parser_to_lowerer_stage = new fe::cst_to_ast_stage{};
	auto typechecker_stage = new fe::typechecker_stage{};
	auto lowering_stage = new fe::lowering_stage{};
	auto interpreting_stage = new fe::interpreting_stage{};

	p
		.lexer(lexing_stage)
		.lexer_to_parser(lexer_to_parser_stage)
		.parser(std::move(parsing_stage))
		.cst_to_ast(parser_to_lowerer_stage)
		.typechecker(typechecker_stage)
		.lowerer(lowering_stage)
		.interpreter(interpreting_stage);

	return p;
}

std::string read_file(const std::string& name)
{
	std::ifstream in(name, std::ios::in | std::ios::binary);
	if (!in) throw std::exception("Could not open file");

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();

	return contents;
}

int main()
{
	auto pipeline = create_pipeline();

	// Load modules
	auto environment = fe::environment{};

	{ // Language module
		auto language_module_contents = read_file("./snippets/language_module.fe");
		auto language_results = pipeline.run_to_interp(std::move(language_module_contents), fe::environment{});
		environment.add_module("language", std::get<2>(language_results));
	}

	// Interpret code
	auto testing_contents = read_file("./snippets/testing.fe");
	auto testing_results = pipeline.run_to_interp(std::move(testing_contents), std::move(environment));
	std::get<1>(testing_results)->print();

	std::cin.get();
}
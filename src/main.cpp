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
#include <string>

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

	auto std_module = fe::environment{};
	{ // Standard module
		std::unique_ptr<fe::types::type> int_type = std::make_unique<fe::types::type>(fe::types::integer_type());

		std_module.set_type("i32", fe::types::meta_type());
		std_module.set_value("i32", std::make_shared<fe::values::type>(fe::types::integer_type()));

		std_module.set_type("str", fe::types::meta_type());
		std_module.set_value("str", std::make_shared<fe::values::type>(fe::types::string_type()));
	}

	auto language_module = fe::environment{};
	{ // Language module
		auto language_module_contents = read_file("./snippets/language_module.fe");

		// std lib
		language_module.add_module("std", std_module);

		auto language_results = pipeline.run_to_interp(std::move(language_module_contents), std::move(language_module));
		language_module = std::get<2>(language_results);
	}

	// Load modules
	auto environment = fe::environment{};
	environment.add_module("std", std_module);
	environment.add_module("language", language_module);

	// Interpret code
	auto testing_contents = read_file("./snippets/testing.fe");
	auto testing_results = pipeline.run_to_interp(std::move(testing_contents), std::move(environment));
	std::get<1>(testing_results)->print();

	std::cin.get();
}
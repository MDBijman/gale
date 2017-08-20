#include "pipeline.h"
#include "ebnfe_parser.h"
#include "reader.h"

#include "lexer_stage.h"
#include "lexer_to_parser_stage.h"
#include "parser_stage.h"
#include "cst_to_ast_stage.h"
#include "typechecker_stage.h"
#include "lowering_stage.h"
#include "interpreting_stage.h"

#include "values.h"

#include "language_module.h"
#include "std_input.h"
#include "std_types.h"
#include "core_operations.h"

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

auto handle_error = []() {

};

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		if(argv[1] == "repl")
		{
			std::cout << "Fe language v0.0 REPL\n";
		}
	}

	auto pipeline = create_pipeline();

	// Load modules
	auto environment = fe::environment{};

	environment.extend(fe::core::operations::load());
	environment.add_module("std", fe::stdlib::core_types::load());
	environment.add_module("language", fe::language_module::load(pipeline));

	// Interpret code
	auto testing_contents = tools::files::read_file("snippets/testing.fe");
	auto results = pipeline.run_to_interp(std::move(testing_contents), std::move(environment));
	if (std::holds_alternative<std::tuple<fe::core_ast::node, fe::values::value, fe::environment>>(results))
	{

	}

	std::cout << std::visit(fe::values::to_string, std::get<fe::values::value>(results)) << std::endl;

	while (1) {
		std::cout << ">>> ";
		std::string line;
		std::getline(std::cin, line);

		if (line == "")
			continue;

		if (line == "env")
		{
			std::cout << environment.to_string() << std::endl;
			continue;
		}

		if (line == "exit")
		{
			exit(0);
		}

		results = pipeline.run_to_interp(std::move(line), std::move(std::get<fe::environment>(results)));
		std::cout << std::visit(fe::values::to_string, std::get<fe::values::value>(results)) << std::endl;

		environment = std::move(std::get<fe::environment>(results));
	}
}
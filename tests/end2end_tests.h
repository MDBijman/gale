#pragma once
#include "pipeline.h"
#include "lexer_stage.h"
#include "lexer_to_parser_stage.h"
#include "parser_stage.h"
#include "cst_to_ast_stage.h"
#include "typechecker_stage.h"
#include "lowering_stage.h"
#include "interpreting_stage.h"

#include <assert.h>

void testTypeDeclaration()
{
	std::string contents = R"delim(x = Type (1 2 ("a" 1 2) () 3 "asd"))delim";

	fe::pipeline p;

	{
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
	}


	std::shared_ptr<fe::value> result = p.process(std::move(contents));
}
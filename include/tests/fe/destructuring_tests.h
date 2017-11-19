#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/lexer_to_parser_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/cst_to_ast_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/interpreting_stage.h"

#include <catch2/catch.hpp>

SCENARIO("destructuring of product values", "[language_feature destructuring]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		auto lexing_stage = new fe::lexing_stage{};
		auto parsing_stage = new fe::parsing_stage{};
		auto lexer_to_parser_stage = new fe::lexer_to_parser_stage{};
		auto parser_to_lowerer_stage = new fe::cst_to_ast_stage{};
		auto typechecker_stage = new fe::typechecker_stage{};
		auto lowering_stage = new fe::lowering_stage{};
		auto interpreting_stage = new fe::interpreting_stage{};

		p
			.lexer(lexing_stage)
			.lexer_to_parser(lexer_to_parser_stage)
			.parser(parsing_stage)
			.cst_to_ast(parser_to_lowerer_stage)
			.typechecker(typechecker_stage)
			.lowerer(lowering_stage)
			.interpreter(interpreting_stage);

		WHEN("a destructuring statement is parsed")
		{
			auto code = 
R"code(
var (a, b, c, _) = (1, 2, 3, 4);
)code";
			auto res = p.process(std::move(code), fe::typecheck_environment{}, fe::runtime_environment{});

			THEN("the variable values should be correct")
			{
				fe::runtime_environment renv = std::get<2>(res);
				auto valueof_a = renv.valueof(fe::core_ast::identifier({}, "a", {}, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_a.get()));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_a.get())->val == 1);
			}
		}
	}
}
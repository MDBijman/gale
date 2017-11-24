#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"

#include <catch2/catch.hpp>

SCENARIO("destructuring of product values", "[language_feature destructuring]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

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
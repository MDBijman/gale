#include <catch2/catch.hpp>

#include "fe/pipeline/pipeline.h"
#include "fe/data/type_environment.h"
#include "fe/data/scope_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/language_definition.h"

SCENARIO("destructuring of product values", "[language_feature destructuring]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		WHEN("a destructuring statement is parsed")
		{
			auto code = 
R"code(
type Quad = (std.i32 a, std.i32 b, std.i32 c, std.i32 d);
var (a, b, c, _): Quad = (1, 2, 3, 4);
)code";
			auto res = p.process(std::move(code), fe::type_environment{}, fe::runtime_environment{}, fe::scope_environment{});

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
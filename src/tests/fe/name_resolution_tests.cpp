#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"

#include <catch2/catch.hpp>

SCENARIO("resolving nested names", "[language_feature name_resolution]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		WHEN("a nested reference is parsed")
		{
			auto code = 
R"code(
module name_resolution_tests
import [std.types]

type Nested = (std.i32 x, std.i32 y);
type Pair = (std.i32 a, Nested m);

var x: Pair = Pair (1, Nested (3, 4));

var z: std.i32 = x.m.x;
)code";
			auto res = p.process(std::move(code), fe::type_environment{}, fe::runtime_environment{}, fe::scope_environment{});

			THEN("the variable types should be correct")
			{
				fe::runtime_environment& renv = std::get<2>(res);
				auto valueof_z = renv.valueof(fe::core_ast::identifier({}, "z", {}, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_z.get()));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_z.get())->val == 3);
			}
		}
	}
}
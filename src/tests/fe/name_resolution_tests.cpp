#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "fe/modes/module.h"
#include "fe/libraries/std/std_types.h"

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
type Nested = (std.i32 x, std.i32 y);
type Pair = (std.i32 a, Nested m);

var x: Pair = Pair (1, Nested (3, 4));

var z: std.i32 = x.m.x;
)code";

			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "x" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			
			auto[te, re, se] = cm.interp(p);

			THEN("the variable types should be correct")
			{
				auto valueof_z = re.valueof(fe::core_ast::identifier({}, "z", {}, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_z.get()));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_z.get())->val == 3);
			}
		}
	}
}
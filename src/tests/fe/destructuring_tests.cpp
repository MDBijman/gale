#include <catch2/catch.hpp>

#include "fe/pipeline/pipeline.h"
#include "fe/data/type_environment.h"
#include "fe/data/scope_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/language_definition.h"
#include "fe/libraries/std/std_types.h"

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
var (a, b, c, _) : Quad = Quad (1, 2, 3, 4);
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm("x", std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			
			auto[te, re, se] = cm.interp(p);

			THEN("the variable values should be correct")
			{
				auto valueof_a = re.valueof(fe::core_ast::identifier({}, "a", {}, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_a.get()));
				REQUIRE(dynamic_cast<fe::values::integer*>(valueof_a.get())->val == 1);
			}
		}
	}
}
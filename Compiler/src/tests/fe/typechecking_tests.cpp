#include <catch2/catch.hpp>

// test utils
#include "tests/test_utils.h"

// lang
#include "fe/modes/project.h"
#include "fe/pipeline/pipeline.h"

// libs
#include "fe/libraries/std/std_io.h"
#include "fe/libraries/std/std_types.h"

TEST_CASE("faulty code typechecking", "[typechecking]")
{
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::io::load());
	p.add_module(fe::stdlib::typedefs::load());

	std::string code = R"code(
import [std std.io]

type Nested = (x: std.i64, y: std.i64);
type Pair = (a: std.i32, m: Nested);

let x: Pair = Pair (1, Nested (3, 4));
)code";
	SECTION("wrong product type")
	{
		auto new_code = code + "let o: Pair = x.m;";
		REQUIRE_THROWS_AS(p.eval(new_code, fe::vm::vm_settings()), fe::typecheck_error);
	}
}

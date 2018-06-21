#include <catch2/catch.hpp>

// test utils
#include "tests/test_utils.h"

// lang
#include "fe/modes/project.h"
#include "fe/pipeline/pipeline.h"

// libs
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_io.h"
#include "fe/libraries/std/std_types.h"

TEST_CASE("resolving nested names", "[name_resolution]")
{
	fe::project p{ fe::pipeline() };
	p.add_module(fe::core::operations::load());
	p.add_module(fe::stdlib::io::load());
	p.add_module(fe::stdlib::typedefs::load());

	std::string code = R"code(
import [std std.io]

type Nested = (x: std.i64 , y: std.i64 );
type Pair = (a: std.i32, m: Nested );

let x: Pair = Pair (1, Nested (3, 4));
let z: std.i64 = x.m.x;
let o: std.i32 = x.a;
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("z", fe::values::i64(3)));
	REQUIRE(scope.value_equals("o", fe::values::i32(1)));
}

TEST_CASE("resolving non-existent names", "[name_resolution]")
{
	fe::project p{ fe::pipeline() };
	p.add_module(fe::core::operations::load());
	p.add_module(fe::stdlib::io::load());
	p.add_module(fe::stdlib::typedefs::load());

	std::string code = R"code(
import [std std.io]

type Nested = (x: std.i64, y: std.i64);
type Pair = (a: std.i32 , m: Nested);

let x: Pair = Pair (1, Nested (3, 4));
)code";

	SECTION("nested access")
	{
		auto new_code = code += "let z: std.i64 = x.m.v;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::resolution_error);
	}

	SECTION("single variable")
	{
		auto new_code = code += "let z: std.i64 = o;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::resolution_error);
	}

	SECTION("unknown type")
	{
		auto new_code = code + "let o: Dummy = x.m;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::resolution_error);
	}
}

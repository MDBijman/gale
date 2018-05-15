#include <catch2/catch.hpp>

// test utils
#include "tests/test_utils.h"

// lang
#include "fe/modes/project.h"
#include "fe/pipeline/pipeline.h"

// libs
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_input.h"
#include "fe/libraries/std/std_output.h"
#include "fe/libraries/std/std_types.h"

TEST_CASE("resolving nested names", "[name_resolution]")
{
	fe::project p{ fe::pipeline() };

	// core
	{
		auto core_scope = fe::core::operations::load();
		p.add_module({ "_core" }, core_scope);
	}

	// std io
	{
		auto i = fe::stdlib::input::load();
		auto o = fe::stdlib::output::load();
		i.merge(std::move(o));
		p.add_module({ "std", "io" }, i);
	}

	// std types
	{
		auto type_scope = fe::stdlib::typedefs::load();
		p.add_module({ "std" }, type_scope);
	}

	std::string code = R"code(
import [std std.io]

type Nested = (std.i64 x, std.i64 y);
type Pair = (std.i32 a, Nested m);

var x: Pair = Pair (1, Nested (3, 4));
var z: std.i64 = x.m.x;
var o: std.i32 = x.a;
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("z", fe::values::i64(3)));
	REQUIRE(scope.value_equals("o", fe::values::i32(1)));
}

TEST_CASE("resolving non-existent names", "[name_resolution]")
{
	fe::project p{ fe::pipeline() };

	// core
	{
		auto core_scope = fe::core::operations::load();
		p.add_module({ "_core" }, core_scope);
	}

	// std io
	{
		auto i = fe::stdlib::input::load();
		auto o = fe::stdlib::output::load();
		i.merge(std::move(o));
		p.add_module({ "std", "io" }, i);
	}

	// std types
	{
		auto type_scope = fe::stdlib::typedefs::load();
		p.add_module({ "std" }, type_scope);
	}

	std::string code = R"code(
import [std std.io]

type Nested = (std.i64 x, std.i64 y);
type Pair = (std.i32 a, Nested m);

var x: Pair = Pair (1, Nested (3, 4));
)code";

	SECTION("nested access")
	{
		auto new_code = code += "var z: std.i64 = x.m.v;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::resolution_error);
	}

	SECTION("single variable")
	{
		auto new_code = code += "var z: std.i64 = o;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::resolution_error);
	}
}

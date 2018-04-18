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

TEST_CASE("integer operations should work with all int types", "[overloading]")
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

	auto code =
		R"code(
import [std]
var a : std.i32 = 1;
var b : std.i32 = 1;
var c : std.i32 = a + b;

var d : std.i64 = 1;
var e : std.i64 = 1;
var f : std.i64 = a + b;
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("a", fe::values::i64(1)));
	REQUIRE(scope.value_equals("b", fe::values::i64(1)));
	REQUIRE(scope.value_equals("c", fe::values::i64(2)));

	REQUIRE(scope.value_equals("d", fe::values::i64(1)));
	REQUIRE(scope.value_equals("e", fe::values::i64(1)));
	REQUIRE(scope.value_equals("f", fe::values::i64(2)));
}

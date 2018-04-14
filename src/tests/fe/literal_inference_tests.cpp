#include <catch2/catch.hpp>

// test utils
#include "tests/test_utils.h"

// lang
#include "fe/modes/project.h"
#include "fe/pipeline/pipeline.h"

// libs
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_types.h"

TEST_CASE("inference of literals", "[literals]")
{
	fe::project p{ fe::pipeline() };

	// core
	{
		auto core_scope = fe::core::operations::load();
		p.add_module({ "_core" }, core_scope);
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
var b : std.i64 = 1;
var c : std.ui32 = 1;
var d : std.ui64 = 1;
var e : std.str = "one";
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("a", fe::values::i32(1)));
	REQUIRE(scope.value_equals("b", fe::values::i64(1)));
	REQUIRE(scope.value_equals("c", fe::values::ui32(1)));
	REQUIRE(scope.value_equals("d", fe::values::ui64(1)));
	REQUIRE(scope.value_equals("e", fe::values::str("one")));
}
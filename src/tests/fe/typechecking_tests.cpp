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

TEST_CASE("faulty code typechecking", "[typechecking]")
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

	SECTION("wrong atom")
	{
		auto new_code = code + "var o: std.i64 = x.a;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::typecheck_error);
	}

	SECTION("wrong product type")
	{
		auto new_code = code + "var o: Pair = x.m;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::typecheck_error);
	}

	SECTION("unknown type")
	{
		auto new_code = code + "var o: Dummy = x.m;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::typecheck_error);
	}
}
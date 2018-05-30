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

type Nested = (x: std.i64, y: std.i64);
type Pair = (a: std.i32, m: Nested);

let x: Pair = Pair (1, Nested (3, 4));
)code";
	SECTION("wrong product type")
	{
		auto new_code = code + "let o: Pair = x.m;";

		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::typecheck_error);
	}
}


TEST_CASE("declaration with tuple type", "[typechecking]")
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
let x : (std.i32, std.i32) = (1, 2);
)code";

	auto res = testing::test_scope(p.eval(std::move(code)));
	std::vector<fe::values::unique_value> x_components;
	x_components.push_back(fe::values::unique_value(new fe::values::i32(1)));
	x_components.push_back(fe::values::unique_value(new fe::values::i32(2)));
	REQUIRE(res.value_equals("x", fe::values::tuple(std::move(x_components))));
}
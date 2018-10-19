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
		p.eval(new_code);
		REQUIRE_THROWS_AS(p.eval(std::move(new_code)), fe::typecheck_error);
	}
}

//TEST_CASE("declaration with tuple type", "[typechecking]")
//{
//	fe::project p{ fe::pipeline() };
//	p.add_module(fe::core::operations::load());
//	p.add_module(fe::stdlib::io::load());
//	p.add_module(fe::stdlib::typedefs::load());
//
//	std::string code = R"code(
//import [std std.io]
//let x : (std.i32, std.i32) = (1, 2);
//)code";
//
//	auto res = testing::test_scope(p.eval(std::move(code)));
//	std::vector<fe::values::unique_value> x_components;
//	x_components.push_back(fe::values::unique_value(new fe::values::i32(1)));
//	x_components.push_back(fe::values::unique_value(new fe::values::i32(2)));
//	REQUIRE(res.value_equals("x", fe::values::tuple(std::move(x_components))));
//}


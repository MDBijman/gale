#include <catch2/catch.hpp>

// test utils
#include "tests/test_utils.h"

// lang
#include "fe/modes/project.h"
#include "fe/pipeline/pipeline.h"

// libs
#include "fe/libraries/std/std_types.h"

//TEST_CASE("inference of literals", "[literals]")
//{
//	fe::project p{ fe::pipeline() };
//	p.add_module(fe::stdlib::typedefs::load());
//
//	auto code =
//		R"code(
//import [std]
//let a : std.i32 = 1;
//let b : std.i64 = 1;
//let c : std.ui32 = 1;
//let d : std.ui64 = 1;
//let e : std.str = "one";
//)code";
//
//	testing::test_scope scope(p.eval(std::move(code)));
//	REQUIRE(scope.value_equals("a", fe::values::i32(1)));
//	REQUIRE(scope.value_equals("b", fe::values::i64(1)));
//	REQUIRE(scope.value_equals("c", fe::values::ui32(1)));
//	REQUIRE(scope.value_equals("d", fe::values::ui64(1)));
//	REQUIRE(scope.value_equals("e", fe::values::str("one")));
//}
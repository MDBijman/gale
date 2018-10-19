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

//TEST_CASE("while loop", "[while]")
//{
//	fe::project p{ fe::pipeline() };
//	p.add_module(fe::core::operations::load());
//	p.add_module(fe::stdlib::io::load());
//	p.add_module(fe::stdlib::typedefs::load());
//
//	auto code = R"code(
//import [std]
//let x : std.i64 = 6;
//while (x > 3) {
//	x = x - 1;
//};
//)code";
//
//	testing::test_scope scope(p.eval(std::move(code)));
//	REQUIRE(scope.value_equals("x", fe::values::i64(3)));
//}

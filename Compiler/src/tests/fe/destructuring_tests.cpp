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

TEST_CASE("destructuring of product values", "[destructuring]")
{
	fe::project p{ fe::pipeline() };
	p.add_module(fe::core::operations::load());
	p.add_module(fe::stdlib::io::load());
	p.add_module(fe::stdlib::typedefs::load());

	auto code =
		R"code(
import [std]
type Quad = (a: std.i64, b: std.i64, c: std.i64, d: std.i64);
let (a, b, c, _) : Quad = Quad (1, 2, 3, 4);
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("a", fe::values::i64(1)));
	REQUIRE(scope.value_equals("b", fe::values::i64(2)));
	REQUIRE(scope.value_equals("c", fe::values::i64(3)));
}

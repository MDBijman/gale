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

TEST_CASE("while loop", "[language_feature][while]")
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

	auto code = R"code(
import [std]
var x : std.i64 = 6;
while (x > 3) {
	x = x - 1;
};
)code";

	testing::test_scope scope(p.eval(std::move(code)));
	REQUIRE(scope.value_equals("x", fe::values::i64(3)));
}

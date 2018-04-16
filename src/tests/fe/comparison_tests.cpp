#include <catch2/catch.hpp>
#include <string>

#include "tests/test_utils.h"
#include "fe/modes/project.h"

#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_input.h"
#include "fe/libraries/std/std_output.h"
#include "fe/libraries/std/std_types.h"


TEST_CASE("comparison operators", "[operators]")
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

var x : std.i64 = 3;
var y : std.i64 = 0;

)code";


	SECTION("equals true")
	{
		std::string equals_code = code + "if (x == 3) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
	}

	SECTION("equals false")
	{
		std::string equals_code = code + "if (x == 4) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
	}

	SECTION("greater than true")
	{
		std::string equals_code = code + "if (x > 2) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
	}

	SECTION("greater than false")
	{
		std::string equals_code = code + "if (x > 4) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
	}

	SECTION("greater or equal than true")
	{
		std::string equals_code = code + "if (x >= 3) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
	}

	SECTION("greater or equal than false")
	{
		std::string equals_code = code + "if (x >= 4) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
	}

	SECTION("smaller than true")
	{
		std::string equals_code = code + "if (x < 4) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
	}

	SECTION("smaller than false")
	{
		std::string equals_code = code + "if (x < 2) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
	}

	SECTION("smaller than or equal true")
	{
		std::string equals_code = code + "if (x <= 3) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
	}

	SECTION("smaller than or equal false")
	{
		std::string equals_code = code + "if (x <= 2) { y = 1; };";
		testing::test_scope scope(p.eval(std::move(equals_code)));
		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
	}
}

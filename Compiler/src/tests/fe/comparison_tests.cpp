#include <catch2/catch.hpp>
#include <string>

#include "tests/test_utils.h"
#include "fe/modes/project.h"

#include "fe/libraries/std/std_io.h"
#include "fe/libraries/std/std_types.h"

//TEST_CASE("comparison operators", "[operators]")
//{
//	fe::project p{ fe::pipeline() };
//	// std io
//	p.add_module(fe::stdlib::io::load());
//	// std types
//	p.add_module(fe::stdlib::typedefs::load());
//
//	std::string code = R"code(
//import [std std.io]
//
//let x : std.i64 = 3;
//let y : std.i64 = 0;
//
//)code";
//
//	SECTION("equals true")
//	{
//		std::string equals_code = code + "if (x == 3) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
//	}
//
//	SECTION("equals false")
//	{
//		std::string equals_code = code + "if (x == 4) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
//	}
//
//	SECTION("greater than true")
//	{
//		std::string equals_code = code + "if (x > 2) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
//	}
//
//	SECTION("greater than false")
//	{
//		std::string equals_code = code + "if (x > 4) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
//	}
//
//	SECTION("greater or equal than true")
//	{
//		std::string equals_code = code + "if (x >= 3) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
//	}
//
//	SECTION("greater or equal than false")
//	{
//		std::string equals_code = code + "if (x >= 4) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
//	}
//
//	SECTION("smaller than true")
//	{
//		std::string equals_code = code + "if (x < 4) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
//	}
//
//	SECTION("smaller than false")
//	{
//		std::string equals_code = code + "if (x < 2) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
//	}
//
//	SECTION("smaller than or equal true")
//	{
//		std::string equals_code = code + "if (x <= 3) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(1)));
//	}
//
//	SECTION("smaller than or equal false")
//	{
//		std::string equals_code = code + "if (x <= 2) { y = 1; };";
//		testing::test_scope scope(p.eval(std::move(equals_code)));
//		REQUIRE(scope.value_equals("y", fe::values::i64(0)));
//	}
//}

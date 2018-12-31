#include <catch2/catch.hpp>
#include <iostream>

#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"
#include "fe/libraries/std/std_assert.h"
#include "tests/test_utils.h"

// Some small helpers

fe::project test_project()
{ 
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::io::load());
	return p;
}

fe::vm::vm_settings test_settings()
{
	return fe::vm::vm_settings(fe::vm::vm_implementation::asm_, false, false, false, false);
}

void expect_io(const std::string& s)
{
	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>(s));
}

// Wraps helpers

void run_with_expectation(const std::string& code, const std::string& out)
{
	expect_io(out);
	test_project().eval(code, test_settings());
}


// Test fib
TEST_CASE("fib", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let fib: std.ui64 -> std.ui64 = \n => if (n <= 2) { 1 } else { (fib (n - 1) + fib (n - 2)) };
let a: std.ui64 = fib 31;
std.io.println a;
)", "1346269");
}

// Test single scope
TEST_CASE("scope", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let a: std.ui64 = 1;
a = {
  let b: std.ui64 = 3;
  b
};
std.io.print a;
)", "3");
}

// Test if
TEST_CASE("if", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let a: std.ui64 = 1;
if (true) { a = 2; } else { a = 3; };
std.io.print a;
)", "2");
}

// Test recursive function call
TEST_CASE("function", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let test: std.ui64 -> std.ui64 = \n => if (n == 1) { n } else { (test (n - 1)) + n };
let a: std.ui64 = test 6;
let b: std.ui64 = a + 2;
std.io.print b;
)", "23");
}

// Test multiple variables in scope
TEST_CASE("vars in block exp", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let x: std.ui64 = {
	let a: std.ui64 = 1;
	let b: std.ui64 = 2;
	let c: std.ui64 = a + b;
	c
};
std.io.print x;
)", "3");
}

TEST_CASE("tuple with 2 elems", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let a : (std.ui64, std.ui64) = (3, 5);
let (b, c): (std.ui64, std.ui64) = a;
std.io.print b;
)", "3");
}

TEST_CASE("tuple with 3 elems", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let a : (std.ui64, std.ui64, std.ui64) = (3, 5, 7);
let (b, c, d): (std.ui64, std.ui64, std.ui64) = a;
std.io.print d;
)", "7");
}

TEST_CASE("vm modules", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [lib std std.io]

let test: std.ui64 = lib.get_ten ();
std.io.print test;
)delim";

	auto p = test_project();
	p.add_module(fe::module_builder()
		.set_name({ "lib" })
		.add_function(
			function("get_ten", bytecode_builder()
				.add(make_mv_reg_i64(ret_reg, 10), make_ret(0))
				.build()),
			fe::types::unique_type(new fe::types::function_type(fe::types::product_type(), fe::types::ui64()))
		).build());

	expect_io("10");
	auto state = p.eval(code, test_settings());
	REQUIRE(state.registers[ret_reg] == 10);
}


TEST_CASE("instructions", "[bytecode]")
{
	using namespace fe::vm;
	auto p = program();
	auto bc = bytecode();
	bc.add_instructions(
		make_mv_reg_ui8(reg(3), 100),
		make_mv_reg_ui16(reg(4), 150),
		make_add(reg(5), reg(3), reg(4)),
		make_mv_reg_ui8(reg(1), 120),
		make_push8(reg(1)),
		make_pop8(reg(2)),
		make_exit()
	);
	p.add_function(function{ "_main", bc, {} });
	auto res = interpret(link(p));
	REQUIRE(res.registers[sp_reg] == 0);
	REQUIRE(res.registers[5] == 250);
	REQUIRE(res.registers[2] == 120);
}

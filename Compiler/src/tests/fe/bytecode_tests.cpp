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
	return fe::vm::vm_settings(fe::vm::vm_implementation::asm_, false , false, false, false);
}

void expect_io(const std::string& s)
{
	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>(s));
}

void finish_io()
{
	auto& io = fe::stdlib::io::get_iostream();
	testing::test_iostream* as_test_io = dynamic_cast<testing::test_iostream*>(&io);
	REQUIRE(as_test_io->has_printed());
}

// Wraps helpers

void run_with_expectation(const std::string& code, const std::string& out)
{
	expect_io(out);
	test_project().eval(code, test_settings());
	finish_io();
}

// Fibonacci

TEST_CASE("fib", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let fib: std.ui64 -> std.ui64 = \n => if (n <= 2) { 1 } else { fib (n - 1) + fib (n - 2) };
let a: std.ui64 = fib 31;
std.io.println a;
)", "1346269\n");
}

// Operators


TEST_CASE("mod", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]
let a: std.ui64 = 300425737568;
let b: std.ui64 = 2;
let c: std.ui64 = a % b;
if(c == 0) {
	std.io.print 0;
} else {
	std.io.print 1;
};
)", "0");
}

TEST_CASE("mod2", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]
let a: std.ui64 = 300425737567;
let b: std.ui64 = 689219;
let c: std.ui64 = a % b;
if(c == 0) {
	std.io.print 0;
} else {
	std.io.print 1;
};
)", "0");
}

TEST_CASE("and short circuit", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]
let a: std.ui64 = 1;
let b: std.ui64 = 2;
let is_false: std.ui64 -> std.bool = \n => { false };
let side_effect: std.ui64 -> std.bool = \n => { std.io.println 3; true };
if(is_false a && side_effect b) {
	std.io.print 0;
} else {
	std.io.print 1;
};
)", "1");
}

TEST_CASE("or short circuit", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]
let a: std.ui64 = 1;
let b: std.ui64 = 2;
let is_true: std.ui64 -> std.bool = \n => { true };
let side_effect: std.ui64 -> std.bool = \n => { std.io.println 3; true };
if(is_true a || side_effect b) {
	std.io.print 1;
} else {
	std.io.print 0;
};
)", "1");
}

// Scoping

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

// If

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

// While

TEST_CASE("while", "[bytecode]")
{
	run_with_expectation(R"(
module fib
import [std std.io]

let a: std.ui64 = 5;
while (a > 2) { a = a - 1; };
std.io.print a;
)", "2");
}



// Recursion

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

// Tuples

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

// Euler problem

TEST_CASE("euler problem 1", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : std.ui64 = 0;
let i : std.ui64 = 1;
while (i < 1000) {
	if ((i % 3) == 0 || (i % 5) == 0) {
		sum = sum + i;
	};
	i = i + 1;
};
std.io.println sum;
)", "233168\n");
}

TEST_CASE("euler problem 2", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let start: [std.ui64; 2] = [1, 2];

let fib: ([std.ui64; 2], std.ui64, std.ui64) -> std.ui64 = \(arr, acc, max) => {
	let next: std.ui64 = (arr!!0) + (arr!!1);
	if (next > max)	{
		acc
	} else {
		let next_arr: [std.ui64; 2] = [arr!!1, next];
		if (next % 2 == 0) { acc = acc + next; };
		fib (next_arr, acc, max)
	}
};

std.io.print (fib ([1, 2], 2, 4000000));

)", "4613732");
};

TEST_CASE("euler problem 3", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let number: std.ui64 = 600851475143;
let divisor: std.ui64 = 2;

while (divisor < number) {
	if(number % divisor == 0) {
		number = number / divisor;
	} else {
		divisor = divisor + 1;
	};
};

std.io.println number;
)", "6857\n");
}

// Sum types

TEST_CASE("sum type int", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool = Num 2;

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print 2; }
};

)", "2");
}

TEST_CASE("sum type bool", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool = Bool true;

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print 2; }
};

)", "1");
}

TEST_CASE("sum type pair", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Pair (3, 3);

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print 2; }
	| Pair x -> { std.io.print 3; }
};

)", "3");
}

TEST_CASE("sum type destruct pair", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Pair (3, 4);

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print 2; }
	| Pair (x, y) -> { std.io.print (x + y); }
};

)", "7");
}

TEST_CASE("sum type match num value", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Num 3;

sum match {
	| Num 3 -> { std.io.print 3; }
	| Num x -> { std.io.print 1; }
};

)", "3");
}

TEST_CASE("sum type match tuple value", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Pair (3, 5);

sum match {
	| Pair (x, 4) -> { std.io.print x; }
	| Pair (3, y) -> { std.io.print y; }
};

)", "5");
}

TEST_CASE("sum type match bool value", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Bool true;

sum match {
	| Num 3 -> { std.io.print 0; }
	| Bool true -> { std.io.print 1; }
	| Bool false -> { std.io.print 2; }
};

)", "1");
}

TEST_CASE("sum type match bool value2", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool | Pair: (std.ui64, std.ui64) = Bool false;

sum match {
	| Num 3 -> { std.io.print 0; }
	| Bool true -> { std.io.print 1; }
	| Bool false -> { std.io.print 2; }
};

)", "2");
}

// Sum type/matching

TEST_CASE("binding variable in match", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool = Num 3;

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print x; }
};

)", "3");
}

TEST_CASE("binding variable in match2", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool = Bool true;

sum match {
	| Bool x -> { std.io.print 1; }
	| Num x -> { std.io.print x; }
};

)", "1");
}

TEST_CASE("binding variable in match3", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let sum : Num: std.ui64 | Bool: std.bool = Bool true;

sum match {
	| Bool x -> if (x) { std.io.print 1; } else { std.io.print 2; }
	| Num x -> { std.io.print x; }
};

)", "1");
}

// Function variable/param

TEST_CASE("vars in function", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let add_one: std.ui64 -> std.ui64 = \n => {
	let x: std.ui64 = 1;
	x + n
};

std.io.print (add_one 5);
)", "6");
}

TEST_CASE("assign param", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let add_one: std.ui64 -> std.ui64 = \n => {
	n = n + 1;
	n
};

std.io.print (add_one 5);
)", "6");
}

TEST_CASE("multiple params", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let add: (std.ui64, std.ui64) -> std.ui64 = \(a, b) => { a + b };

std.io.print (add (5, 5));
)", "10");
}

// Array

TEST_CASE("first elem of array parameter", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let y: [std.ui64; 3] = [1,2,3];

let first: [std.ui64; 3] -> std.ui64 = \arr => (arr!!0);

std.io.print (first y);

)", "1");
}

TEST_CASE("second elem of array parameter", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let y: [std.ui64; 3] = [1,2,3];

let second: [std.ui64; 3] -> std.ui64 = \arr => (arr!!1);

std.io.print (second y);

)", "2");
}

TEST_CASE("third elem of array parameter", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let y: [std.ui64; 3] = [1,2,3];

let third: [std.ui64; 3] -> std.ui64 = \arr => (arr!!2);

std.io.print (third y);

)", "3");
}

TEST_CASE("first array elem", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let a : [std.ui64; 3] = [1, 2, 3];
std.io.print (a!!0);
)", "1");
}

TEST_CASE("second array elem", "[bytecode]")
{
	run_with_expectation(R"(
module test
import [std std.io]

let a : [std.ui64; 3] = [1, 2, 3];
std.io.print (a!!1);
)", "2");
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
	auto executable = link(p);
	auto res = interpret(executable);
	REQUIRE(res.registers[sp_reg] == 0);
	REQUIRE(res.registers[5] == 250);
	REQUIRE(res.registers[2] == 120);
}

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


TEST_CASE("assert true", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.io std.assert]
		)delim";

	using namespace fe;
	project p{ pipeline() };
	p.add_module(stdlib::typedefs::load());
	p.add_module(stdlib::assert::load());
	p.add_module(stdlib::io::load());
	REQUIRE_NOTHROW(p.eval(code, vm::vm_settings()));
}

TEST_CASE("fib", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.io]

let fib: std.ui64 -> std.ui64 = \n => if (n <= 2) { 1 } else { (fib (n - 1) + fib (n - 2)) };
let a: std.ui64 = fib 31;
std.io.println a;

)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::io::load());
	auto before = std::chrono::high_resolution_clock::now();

	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>("1346269"));
	p.eval(code, fe::vm::vm_settings(fe::vm::vm_implementation::asm_, false , false, false));
	auto after = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration<double, std::milli>(after - before).count();
	std::cout << time << std::endl;
}

TEST_CASE("scope", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.io]

let a: std.ui64 = 1;
a = 2;
std.io.print a;
)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.add_module(fe::stdlib::io::load());
	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>("2"));
	p.eval(code, fe::vm::vm_settings(fe::vm::vm_implementation::asm_, false, false, false));
	fe::stdlib::io::set_iostream(std::make_unique<fe::stdlib::io::iostream>());
}
TEST_CASE("if", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.io]

let a: std.ui64 = 1;
if (true) { a = 2; } else { a = 3; };
std.io.print a;
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::io::load());
	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>("2"));
	p.eval(code, fe::vm::vm_settings(fe::vm::vm_implementation::asm_, true, true, false));
}

//TEST_CASE("vm modules", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto code = R"delim(
//module test
//import [lib std std.io]
//
//let test: std.ui64 = lib.get_ten ();
//std.print test;
//)delim";
//
//	fe::project p{ fe::pipeline() };
//
//	p.add_module(fe::stdlib::typedefs::load());
//	p.add_module(fe::stdlib::io::load());
//	p.add_module(fe::module_builder()
//		.set_name({ "lib" })
//		.add_function(
//			function("get_ten", bytecode_builder()
//				.add(make_mv_reg_i64(ret_reg, 10), make_ret(0))
//				.build()),
//			fe::types::unique_type(new fe::types::function_type(fe::types::product_type(), fe::types::ui64()))
//		).build());
//
//	fe::stdlib::io::set_iostream(std::make_unique<testing::test_iostream>("10"));
//	auto state = p.eval(code, fe::vm::vm_settings());
//	REQUIRE(state.registers[ret_reg] == 10);
//}

TEST_CASE("function", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [std std.io]

let test: std.ui64 -> std.ui64 = \n => if (n == 1) { n } else { (test (n - 1)) + n };
let a: std.ui64 = test 6;
let b: std.ui64 = a + 2;
std.io.print b;
)delim";

	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::io::load());
	fe::stdlib::io::set_iostream(std::make_unique<fe::stdlib::io::iostream>());
	p.eval(code, fe::vm::vm_settings(fe::vm::vm_implementation::asm_, true, false, false));
}

//TEST_CASE("instructions", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto p = program();
//	auto bc = bytecode();
//	bc.add_instructions(
//		make_mv_reg_ui8(reg(3), 100),
//		make_mv_reg_ui16(reg(4), 150),
//		make_add(reg(5), reg(3), reg(4)),
//		make_mv_reg_ui8(reg(1), 120),
//		make_push8(reg(1)),
//		make_pop8(reg(2))
//	);
//	p.add_function(function{ "_main", bc, {} });
//	auto res = interpret(link(p));
//	REQUIRE(res.registers[sp_reg] == 0);
//	REQUIRE(res.registers[5] == 250);
//	REQUIRE(res.registers[2] == 120);
//}

TEST_CASE("number", "[bytecode]")
{
	using namespace fe::vm;
	auto ast = fe::core_ast::ast(fe::core_ast::node_type::BLOCK);
	auto root = ast.root_id();
	auto num = ast.create_node(fe::core_ast::node_type::NUMBER, root);
	auto& num_node = ast.get_node(num);
	ast.get_data<fe::number>(*num_node.data_index).value = 10;

	auto res = interpret(link(generate_bytecode(ast)));
	REQUIRE(res.registers[sp_reg] == 0);
}

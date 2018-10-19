#include <catch2/catch.hpp>
#include <iostream>

#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"
#include "fe/libraries/std/std_assert.h"

TEST_CASE("fib", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.assert]

let fib: std.i64 -> std.i64 = \n => if (n <= 2) { 1 } else { (fib (n - 1) + fib (n - 2)) };
let a: std.i64 = fib 31;
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	auto before = std::chrono::high_resolution_clock::now();
	p.eval(code, fe::vm::vm_settings());
	auto after = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration<double, std::milli>(after - before).count();
	std::cout << time << std::endl;
}

TEST_CASE("scope", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.assert]

let a: std.i64 = 1;
{
	a = 2;
};
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.eval(code, fe::vm::vm_settings());
}

TEST_CASE("assignment", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.assert]

let a: std.i64 = 1;
a = 2;
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.eval(code, fe::vm::vm_settings());
}

TEST_CASE("if", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.assert]

let a: std.i64 = 1;
if (true) { a = 2; } else { a = 3; };
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.eval(code, fe::vm::vm_settings());
}

TEST_CASE("vm modules", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [lib std]

let test: std.i64 = lib.get_ten ();
)delim";

	fe::project p{ fe::pipeline() };

	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::module_builder()
		.set_name({ "lib" })
		.add_function(
			function("get_ten", bytecode_builder()
				.add(make_mv_reg_i64(ret_reg, 10), make_ret(0))
				.build()),
			fe::types::unique_type(new fe::types::function_type(fe::types::product_type(), fe::types::i64()))
		).build());

	auto state = p.eval(code, fe::vm::vm_settings());
	REQUIRE(state.registers[ret_reg] == 10);
}

TEST_CASE("variable", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [std std.assert]

let a: std.i64 = 3;
)delim";

	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.eval(code, fe::vm::vm_settings());
}

TEST_CASE("function", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [std std.assert]

let test: std.i64 -> std.i64 = \n => n;
let a: std.i64 = test 3;
let b: std.i64 = a + 2;
)delim";

	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	p.eval(code, fe::vm::vm_settings());
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
		make_pop8(reg(2))
	);
	p.add_function(function{ "_main", bc, {} });
	auto res = interpret(link(p));
	REQUIRE(res.registers[sp_reg] == 0);
	REQUIRE(res.registers[5] == 250);
	REQUIRE(res.registers[2] == 120);
}

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

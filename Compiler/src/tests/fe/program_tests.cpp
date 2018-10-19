#include <catch2/catch.hpp>
#include <iostream>

#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"
#include "fe/libraries/std/std_assert.h"

TEST_CASE("fib program", "[program]")
{

	auto code = R"delim(

module fib
import [std std.assert]

let fib: std.ui64 -> std.ui64 = \n => if (n <= 2) { 1 } else { (fib (n - 1)) + (fib (n - 2)) };
let a: std.ui64 = fib 35;
		)delim";

	using namespace fe::types;
	fe::project p{ fe::pipeline() };
	p.add_module(fe::stdlib::typedefs::load());
	p.add_module(fe::stdlib::assert::load());
	auto state = p.eval(code);
	REQUIRE(state.registers[60] == 9227465);
}
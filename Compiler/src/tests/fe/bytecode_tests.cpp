#include <catch2/catch.hpp>
#include <iostream>

#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"

TEST_CASE("fib", "[bytecode]")
{
	auto code = R"delim(
module fib
import [std std.io]

let fib: std.i64 -> std.i64 = \n => if (n <= 2) { 1 } else { (fib (n - 1) + fib (n - 2)) };
let a: std.i64 = fib 3;
		)delim";

	fe::project p{ fe::pipeline() };
	// std io
	p.add_module(fe::stdlib::io::load());
	// std ui
	p.add_module(fe::stdlib::ui::load());
	// std types
	p.add_module(fe::stdlib::typedefs::load());
	auto mod = p.eval(code);
}


TEST_CASE("interrupt", "[bytecode]")
{
	using namespace fe::vm;
	auto p = program();
	auto bc = bytecode();

	auto interrupt = [](machine_state& s) {
		s.registers[ret_reg] = 10;
	};
	auto id = p.add_interrupt(interrupt);

	bc.add_instruction(make_int(id));
	p.add_chunk(bc);

	auto res = interpret(p);

	REQUIRE(res.registers[ret_reg] == 10);
}

TEST_CASE("function", "[bytecode]")
{
	using namespace fe::vm;
	auto code = R"delim(
module test
import [std]

let test: std.i64 -> std.i64 = \n => n;
let a: std.i64 = test 3;
let b: std.i64 = a + 2;
		)delim";

	fe::project p{ fe::pipeline() };
	// std types
	p.add_module(fe::stdlib::typedefs::load());
	auto state = p.eval(code);
	REQUIRE(state.registers[sp_reg] == 0);
	REQUIRE(state.registers[ret_reg] == 3);
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
	p.add_chunk(bc);
	auto res = interpret(p);
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

	auto p = generate_bytecode(ast);
	auto res = interpret(p);
	REQUIRE(res.registers[sp_reg] == 0);
}

//
//TEST_CASE("while", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto ast = fe::core_ast::ast(fe::core_ast::node_type::BLOCK);
//	auto root = ast.root_id();
//
//	{
//		auto a_init = ast.create_node(fe::core_ast::node_type::SET, root);
//		auto lhs_id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, a_init);
//		ast.get_data<fe::core_ast::identifier>(*(ast.get_node(lhs_id).data_index)).variable_name = "a";
//		ast.get_node(lhs_id).size = 8;
//		auto num_id = ast.create_node(fe::core_ast::node_type::NUMBER, a_init);
//		auto& num1 = ast.get_data<fe::number>(*(ast.get_node(num_id).data_index));
//		num1.type = fe::number_type::I64;
//		num1.value = 8;
//	}
//
//	{
//		auto loop = ast.create_node(fe::core_ast::node_type::WHILE_LOOP, root);
//
//		auto conditional = ast.create_node(fe::core_ast::node_type::GT, loop);
//		auto& cn = ast.get_node(conditional);
//		auto cn_id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, conditional);
//		ast.get_node(cn_id).size = 8;
//		ast.get_data<fe::core_ast::identifier>(*(ast.get_node(cn_id).data_index)).variable_name = "a";
//		auto num_id = ast.create_node(fe::core_ast::node_type::NUMBER, conditional);
//		auto& num2 = ast.get_data<fe::number>(*(ast.get_node(num_id).data_index)); 
//		num2.type = fe::number_type::I64;
//		num2.value = 2;
//
//		{
//			auto body = ast.create_node(fe::core_ast::node_type::SET, loop);
//			auto lhs_id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, body);
//			ast.get_data<fe::core_ast::identifier>(*(ast.get_node(lhs_id).data_index)).variable_name = "a";
//
//			auto sub_id = ast.create_node(fe::core_ast::node_type::SUB, body);
//			auto rhs_id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, sub_id);
//			ast.get_node(rhs_id).size = 8;
//			ast.get_data<fe::core_ast::identifier>(*(ast.get_node(rhs_id).data_index)).variable_name = "a";
//			auto num_id = ast.create_node(fe::core_ast::node_type::NUMBER, sub_id);
//			auto& num3 = ast.get_data<fe::number>(*(ast.get_node(num_id).data_index)); 
//			num3.type = fe::number_type::I64;
//			num3.value = 1;
//		}
//	}
//
//	auto p = generate_bytecode(ast);
//	auto res = interpret(p);
//}
//
//TEST_CASE("identifier", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto ast = fe::core_ast::ast(fe::core_ast::node_type::BLOCK);
//	auto root = ast.root_id();
//
//	{
//		auto set = ast.create_node(fe::core_ast::node_type::SET, root);
//		auto& set_node = ast.get_node(set);
//
//		auto lhs = ast.create_node(fe::core_ast::node_type::IDENTIFIER, set);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(lhs).data_index).variable_name = "a";
//		ast.get_node(lhs).size = 8;
//		auto rhs = ast.create_node(fe::core_ast::node_type::NUMBER, set);
//		ast.get_data<fe::number>(*ast.get_node(rhs).data_index) = { 10, fe::number_type::I64 };
//	}
//
//	{
//		auto get = ast.create_node(fe::core_ast::node_type::IDENTIFIER, root);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(get).data_index).variable_name = "a";
//		ast.get_node(get).size = 8;
//	}
//
//	auto p = generate_bytecode(ast);
//	auto res = interpret(p);
//}
//
//TEST_CASE("identity function", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto ast = fe::core_ast::ast(fe::core_ast::node_type::BLOCK);
//	auto root = ast.root_id();
//
//	{
//		auto set = ast.create_node(fe::core_ast::node_type::SET, root);
//		auto& set_node = ast.get_node(set);
//
//		auto lhs = ast.create_node(fe::core_ast::node_type::IDENTIFIER, set);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(lhs).data_index).variable_name = "a";
//		auto fn = ast.create_node(fe::core_ast::node_type::FUNCTION, set);
//		ast.get_node(fn).size = 8;
//		{
//			auto lhs = ast.create_node(fe::core_ast::node_type::IDENTIFIER, fn);
//			ast.get_data<fe::core_ast::identifier>(*ast.get_node(lhs).data_index).variable_name = "b";
//			ast.get_node(lhs).size = 8;
//			auto body = ast.create_node(fe::core_ast::node_type::IDENTIFIER, fn);
//			ast.get_data<fe::core_ast::identifier>(*ast.get_node(body).data_index).variable_name = "b";
//			ast.get_node(body).size = 8;
//		}
//	}
//
//	{
//		auto call = ast.create_node(fe::core_ast::node_type::FUNCTION_CALL, root);
//		ast.get_node(call).size = 8;
//		auto id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, call);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(id).data_index).variable_name = "a";
//		ast.get_node(id).size = 8;
//		auto param = ast.create_node(fe::core_ast::node_type::NUMBER, call);
//		ast.get_data<fe::number>(*ast.get_node(param).data_index) = { 10, fe::number_type::I64 };
//	}
//
//	auto p = generate_bytecode(ast);
//	auto res = interpret(p);
//}
//
//TEST_CASE("identity function two params", "[bytecode]")
//{
//	using namespace fe::vm;
//	auto ast = fe::core_ast::ast(fe::core_ast::node_type::BLOCK);
//	auto root = ast.root_id();
//
//	{
//		auto set = ast.create_node(fe::core_ast::node_type::SET, root);
//		auto& set_node = ast.get_node(set);
//
//		auto lhs = ast.create_node(fe::core_ast::node_type::IDENTIFIER, set);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(lhs).data_index).variable_name = "a";
//		auto fn = ast.create_node(fe::core_ast::node_type::FUNCTION, set);
//		ast.get_node(fn).size = 8;
//		{
//			auto params = ast.create_node(fe::core_ast::node_type::IDENTIFIER_TUPLE, fn);
//			{
//				auto first_param = ast.create_node(fe::core_ast::node_type::IDENTIFIER, params);
//				ast.get_data<fe::core_ast::identifier>(*ast.get_node(first_param).data_index).variable_name = "b";
//				ast.get_node(first_param).size = 8;
//				auto second_param = ast.create_node(fe::core_ast::node_type::IDENTIFIER, params);
//				ast.get_data<fe::core_ast::identifier>(*ast.get_node(second_param).data_index).variable_name = "c";
//				ast.get_node(second_param).size = 8;
//			}
//			auto body = ast.create_node(fe::core_ast::node_type::TUPLE, fn);
//			{
//				auto first_elem = ast.create_node(fe::core_ast::node_type::IDENTIFIER, body);
//				ast.get_data<fe::core_ast::identifier>(*ast.get_node(first_elem).data_index).variable_name = "b";
//				ast.get_node(first_elem).size = 8;
//				auto second_elem = ast.create_node(fe::core_ast::node_type::IDENTIFIER, body);
//				ast.get_data<fe::core_ast::identifier>(*ast.get_node(second_elem).data_index).variable_name = "c";
//				ast.get_node(second_elem).size = 8;
//			}
//		}
//	}
//
//	{
//		auto call = ast.create_node(fe::core_ast::node_type::FUNCTION_CALL, root);
//		ast.get_node(call).size = 8;
//		auto id = ast.create_node(fe::core_ast::node_type::IDENTIFIER, call);
//		ast.get_data<fe::core_ast::identifier>(*ast.get_node(id).data_index).variable_name = "a";
//
//		auto params = ast.create_node(fe::core_ast::node_type::TUPLE, call);
//
//		auto param1 = ast.create_node(fe::core_ast::node_type::NUMBER, params);
//		ast.get_data<fe::number>(*ast.get_node(param1).data_index) = { 16, fe::number_type::I64 };
//
//		auto param2 = ast.create_node(fe::core_ast::node_type::NUMBER, params);
//		ast.get_data<fe::number>(*ast.get_node(param2).data_index) = { 8, fe::number_type::I64 };
//	}
//
//	auto p = generate_bytecode(ast);
//	auto res = interpret(p);
//}

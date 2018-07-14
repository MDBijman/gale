#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"

namespace fe::vm
{
	void code_gen_state::link_node_chunk(core_ast::node_id n, uint8_t c)
	{
		node_to_chunk.insert_or_assign(n, c);
	}

	uint8_t code_gen_state::chunk_of(core_ast::node_id n)
	{
		assert(node_to_chunk.find(n) != node_to_chunk.end());
		return node_to_chunk.at(n);
	}

	bool code_gen_state::is_mapped(const core_ast::identifier& id)
	{
		return identifier_variables.find(id) != identifier_variables.end();
	}


	code_gen_info::code_gen_info() : result_size(0), result_location(0), code_location() {}
	code_gen_info::code_gen_info(uint8_t size, variable_location res_loc, far_lbl loc) 
		: result_size(size), result_location(res_loc), code_location(loc) {}
}

namespace fe::vm
{
	code_gen_info generate_bytecode(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i);

	void link_to_parent_chunk(core_ast::node_id n, core_ast::ast& ast, code_gen_state& i)
	{
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
	}

	code_gen_info generate_number(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i) 
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));

		auto& num = ast.get_data<fe::number>(*ast.get_node(n).data_index);
		size_t size = 0;
		reg r_result = i.alloc_register();
		switch (num.type)
		{
		case number_type::UI32: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<uint32_t>(num.value)));
			return code_gen_info(4, r_result, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::I32: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<int32_t>(num.value)));
			return code_gen_info(4, r_result, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::UI64: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<uint64_t>(num.value)));
			return code_gen_info(8, r_result, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::I64: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<int64_t>(num.value)));
			return code_gen_info(8, r_result, far_lbl(i.chunk_of(n), location.ip));
		}
		default: assert(!"Number type not supported");
		}

	}
	code_gen_info generate_string(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI string");
	}
	code_gen_info generate_boolean(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));

		auto& value = ast.get_data<fe::boolean>(*ast.get_node(n).data_index).value;
		auto r_res = i.alloc_register();
		auto location = bc.add_instruction(make_mv(r_res, value ? 1 : 0));

		return code_gen_info(1, r_res, far_lbl(i.chunk_of(n), location.ip));
	}
	code_gen_info generate_identifier(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& node = ast.get_node(n);
		assert(node.kind == core_ast::node_type::IDENTIFIER);

		auto r_address = i.alloc_register();
		auto r_offset = i.alloc_register();
		auto r_target = i.alloc_register();

		auto& chunk = p.get_chunk(i.chunk_of(n));
		auto location = chunk.add_instruction(make_load_sp(r_address));
		chunk.add_instruction(make_mv(r_offset, 10));
		chunk.add_instruction(make_sub(r_address, r_address, r_offset));
		chunk.add_instruction(make_load64(r_target, r_address));
		return code_gen_info(8, r_target, far_lbl(i.chunk_of(n), location.ip));
	}
	code_gen_info generate_identifier_tuple(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI id tuple");
	}
	code_gen_info generate_set(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& node = ast.get_node(n);
		assert(node.children.size() == 2);
		assert(node.kind == core_ast::node_type::SET);

		auto& lhs = ast.get_node(node.children[0]);
		auto& id = ast.get_data<core_ast::identifier>(*lhs.data_index);

		// Find register to put it in
		reg r = i.alloc_register();
		auto& rhs_id = node.children[1];

		// #robustness 
		// Assumes that the generated function creates a new chunk with id == p.nr_of_chunks() at ip == 0
		//if (ast.get_node(rhs_id).kind == core_ast::node_type::FUNCTION)
		//	i.add_function(id, far_lbl(p.nr_of_chunks(), 0));

		auto info = generate_bytecode(rhs_id, ast, p, i);

		if (ast.get_node(rhs_id).kind != core_ast::node_type::FUNCTION 
			&& std::holds_alternative<memory_location>(info.result_location))
		{
			p.get_chunk(i.chunk_of(n)).add_instruction(make_pop(info.result_size, r));
		}

		return code_gen_info();
	}
	code_gen_info generate_function(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto id = p.add_chunk(bytecode());
		auto& func_bc = p.get_chunk(id);
		i.link_node_chunk(n, id);

		auto& node = ast.get_node(n);
		assert(node.children.size() == 2);

		auto used_registers = i.clear_registers();
		for (reg r : used_registers)
			func_bc.add_instruction(make_push64(r));

		// Generate body
		auto i2 = generate_bytecode(node.children[1], ast, p, i);

		for (reg r : used_registers)
			func_bc.add_instruction(make_pop64(r));

		auto& lhs = ast.get_node(node.children[0]);
		p.get_chunk(id).add_instruction(make_ret(*lhs.size, i2.result_size));

		return code_gen_info(i2.result_size, i2.result_location, far_lbl(id, 0));
	}
	code_gen_info generate_tuple(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		link_to_parent_chunk(n, ast, info);

		auto& node = ast.get_node(n);
		uint8_t bytes_sum = 0;
		auto first_info = generate_bytecode(node.children[0], ast, p, info);

		for (auto i = 1; i < node.children.size(); i++)
			bytes_sum += generate_bytecode(node.children[i], ast, p, info).result_size;

		return code_gen_info(bytes_sum, far_lbl(info.chunk_of(n), first_info.code_location.ip));
	}
	code_gen_info generate_block(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		auto chunk_id = 0;
		if (ast.root_id() == n)
		{
			chunk_id = p.add_chunk(bytecode());
			info.link_node_chunk(n, chunk_id);
		}
		else
		{
			link_to_parent_chunk(n, ast, info);
			chunk_id = info.chunk_of(n);
		}

		auto& children = ast.children_of(n);

		if (children.size() > 1)
		{
			// First elem
			auto r_sink = info.empty_register();
			auto first_info = generate_bytecode(children[0], ast, p, info);
			for (auto i = 0; i < first_info.allocated_stack_bytes; i++)
				p.get_chunk(chunk_id).add_instruction(make_pop8(r_sink));

			// Middle elems
			for (auto i = 1; i < children.size() - 1; i++)
			{
				auto i1 = generate_bytecode(children[i], ast, p, info);

				r_sink = info.empty_register();
				for (auto j = 0; j < i1.allocated_stack_bytes; j++)
					p.get_chunk(chunk_id).add_instruction(make_pop8(r_sink));
			}

			// Last elem
			auto last_info = generate_bytecode(children.back(), ast, p, info);

			return code_gen_info(last_info.allocated_stack_bytes, first_info.location);
		}
		else
		{
			auto first_info = generate_bytecode(children[0], ast, p, info);
			return code_gen_info(first_info.allocated_stack_bytes, first_info.location);
		}
	}
	code_gen_info generate_function_call(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		assert(node.children.size() == 2);
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));

		auto& id_node = ast.get_node(node.children[0]);
		auto& id = ast.get_data<core_ast::identifier>(*id_node.data_index);
		far_lbl func_loc = i.get_function_loc(id);

		// Put params on stack
		auto& param_node = ast.get_node(node.children[1]);
		auto i1 = generate_bytecode(param_node.id, ast, p, i);

		// Perform call, putting return address on stack
		auto r_target_bc = i.empty_register();
		auto r_target_ip = i.empty_register();
		auto& c = p.get_chunk(i.chunk_of(n));
		c.add_instructions({
			make_mv(r_target_bc, func_loc.chunk_id),
			make_mv(r_target_ip, func_loc.ip),
			make_call(r_target_bc, r_target_ip)
			});

		assert(node.size);
		return code_gen_info(*node.size, far_lbl(i.chunk_of(n), i1.location.ip));
	}
	code_gen_info generate_branch(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		link_to_parent_chunk(n, ast, info);
		auto& bc = p.get_chunk(info.chunk_of(n));

		auto& node = ast.get_node(n);
		far_lbl first_loc;
		uint8_t bytes = 0;

		auto v_after_body_jmp_target = info.allocate_variable();
		auto r_after_body_jmp_target = info.load(v_after_body_jmp_target);
		std::vector<near_lbl> after_body_jmps;
		for (auto i = 0; i < node.children.size(); i += 2)
		{
			auto& cond = ast.get_node(node.children[i]);
			auto& body = ast.get_node(node.children[i + 1]);

			// Conditional
			auto i1 = generate_bytecode(cond.id, ast, p, info);
			if (i == 0) first_loc = i1.location;
			auto r_cond = info.empty_register();
			auto r_target = info.empty_register();
			p.get_chunk(info.chunk_of(n)).add_instruction(make_pop(i1.allocated_stack_bytes, r_cond));
			auto mv_loc = p.get_chunk(info.chunk_of(n)).add_instruction(make_mv(r_target, 0));
			p.get_chunk(info.chunk_of(n)).add_instruction(make_jz(r_cond, r_target));

			// Body
			auto i2 = generate_bytecode(body.id, ast, p, info);
			if (i == 0) bytes = i2.allocated_stack_bytes;
			else assert(bytes == i2.allocated_stack_bytes);
			// After body jump to after all branches
			r_after_body_jmp_target = info.load(v_after_body_jmp_target);
			auto after_body = bc.add_instructions({
				make_mv(r_after_body_jmp_target, 0 /* set later */),
				make_jmp(r_after_body_jmp_target)
				});
			after_body_jmps.push_back(after_body);

			// Target address for false conditional
			auto after_loc = p.get_chunk(info.chunk_of(n)).add_instruction(make_nop());
			// Set conditional target
			p.get_chunk(info.chunk_of(n)).set_instruction(mv_loc + 2, single_byte({ static_cast<uint8_t>(after_loc.ip) }));
		}
		// Set after body targets
		auto after_all_loc = bc.add_instruction(make_nop());
		r_after_body_jmp_target = info.load(v_after_body_jmp_target);
		for (auto lbl : after_body_jmps)
			bc.set_instruction(lbl.ip, make_mv(r_after_body_jmp_target, after_all_loc.ip));

		return code_gen_info(bytes, first_loc);
	}
	code_gen_info generate_reference(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{

		throw std::runtime_error("NYI ref");
	}
	code_gen_info generate_while_loop(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));
		auto& children = ast.children_of(n);

		// a) test conditional, if false jmp to (d) 
		auto before = bc.add_instruction(single_byte{ op_to_byte(op_kind::LBL) });
		generate_bytecode(children[0], ast, p, i);

		auto r_tst = i.empty_register();
		auto r_jmp_false = i.empty_register();
		auto if_false_jmp = bc.add_instructions({
			double_byte{ op_to_byte(op_kind::POP8_REG), r_tst.val },
			triple_byte{ op_to_byte(op_kind::MV_REG_UI8), r_jmp_false.val, 0 /* set later */ },
			triple_byte{ op_to_byte(op_kind::JZ_REG_REG), r_tst.val, r_jmp_false.val }
			});

		// b) body
		auto r_sink = i.empty_register();
		auto body_info = generate_bytecode(children[1], ast, p, i);
		for (auto i = 0; i < body_info.allocated_stack_bytes; i++)
			bc.add_instruction(make_pop8(r_sink));

		// c) jmp back to (a), test conditional
		auto r_jmp_cond = i.empty_register();
		bc.add_instructions({
			triple_byte{ op_to_byte(op_kind::MV_REG_UI8), r_jmp_cond.val, static_cast<uint8_t>(before.ip) },
			double_byte{ op_to_byte(op_kind::JMP_REG), r_jmp_cond.val}
			});

		// d) target lbl for false conditional jmp from (a)
		auto after = bc.add_instruction(single_byte{ op_to_byte(op_kind::LBL) });

		// set jmp target of (a) to lbl of (d)
		bc.set_instruction(if_false_jmp + 2, triple_byte{ op_to_byte(op_kind::MV_REG_UI8), r_jmp_false.val, static_cast<uint8_t>(after.ip) });

		return code_gen_info(0, far_lbl(i.chunk_of(n), before.ip));
	}

	code_gen_info generate_binary_op(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));
		auto& children = ast.children_of(n);

		// children bytecode should leave 2 values on stack
		auto i1 = generate_bytecode(children[0], ast, p, i);
		assert(i1.allocated_stack_bytes <= 8);
		auto i2 = generate_bytecode(children[1], ast, p, i);
		assert(i2.allocated_stack_bytes <= 8);

		auto r_res_lhs = i.empty_register();
		auto r_res_rhs = i.empty_register();
		auto info = bc.add_instructions({
			make_pop(i1.allocated_stack_bytes, r_res_rhs),
			make_pop(i2.allocated_stack_bytes, r_res_lhs),
			});
		auto r_res = i.empty_register();

		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::ADD: bc.add_instruction(make_add(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::SUB: bc.add_instruction(make_sub(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::MUL: bc.add_instruction(make_mul(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::DIV: bc.add_instruction(make_div(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::MOD: bc.add_instruction(make_mod(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::AND: bc.add_instruction(make_and(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::OR:  bc.add_instruction(make_or(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::LT:  bc.add_instruction(make_gte(r_res, r_res_rhs, r_res_lhs)); break;
		case core_ast::node_type::GT:  bc.add_instruction(make_gt(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::LEQ: bc.add_instruction(make_gt(r_res, r_res_rhs, r_res_lhs)); break;
		case core_ast::node_type::GEQ: bc.add_instruction(make_gte(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::EQ:  bc.add_instruction(make_eq(r_res, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::NEQ: bc.add_instruction(make_neq(r_res, r_res_lhs, r_res_rhs)); break;
		}
		auto size = i1.allocated_stack_bytes > i2.allocated_stack_bytes
			? i1.allocated_stack_bytes
			: i2.allocated_stack_bytes;
		bc.add_instruction(make_push(size, r_res));

		return code_gen_info(size, far_lbl(i.chunk_of(n), info.ip));
	}

	code_gen_info generate_bytecode(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::NOP: return code_gen_info();
		case core_ast::node_type::NUMBER: return generate_number(n, ast, p, i);
		case core_ast::node_type::STRING: return generate_string(n, ast, p, i);
		case core_ast::node_type::BOOLEAN: return generate_boolean(n, ast, p, i);
		case core_ast::node_type::IDENTIFIER: return generate_identifier(n, ast, p, i);
		case core_ast::node_type::IDENTIFIER_TUPLE: return generate_identifier_tuple(n, ast, p, i);
		case core_ast::node_type::SET: return generate_set(n, ast, p, i);
		case core_ast::node_type::FUNCTION: return generate_function(n, ast, p, i);
		case core_ast::node_type::TUPLE: return generate_tuple(n, ast, p, i);
		case core_ast::node_type::BLOCK: return generate_block(n, ast, p, i);
		case core_ast::node_type::FUNCTION_CALL: return generate_function_call(n, ast, p, i);
		case core_ast::node_type::BRANCH: return generate_branch(n, ast, p, i);
		case core_ast::node_type::REFERENCE: return generate_reference(n, ast, p, i);
		case core_ast::node_type::WHILE_LOOP: return generate_while_loop(n, ast, p, i);
		default:
			if (core_ast::is_binary_op(node.kind)) return generate_binary_op(n, ast, p, i);
			throw std::runtime_error("Unknown node type");
		}
	}

	program generate_bytecode(core_ast::ast& n)
	{
		// Program that will contain the chunks containing the bytecode
		program p;
		// Meta information about intersection between core_ast and bytecode e.g. ast to chunk mapping etc.
		code_gen_state i;
		auto root_id = n.root_id();
		generate_bytecode(root_id, n, p, i);
		return p;
	}
}

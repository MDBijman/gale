#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"

namespace fe::vm
{
	std::vector<reg> code_gen_state::clear_registers()
	{
		auto copy = this->in_use;
		this->in_use.clear();
		return copy;
	}
	
	reg code_gen_state::alloc_register()
	{
		reg res = this->next_allocation;
		this->next_allocation.val++;
		return res;
	}

	void code_gen_state::link_node_chunk(core_ast::node_id n, uint8_t c)
	{
		node_to_chunk.insert_or_assign(n, c);
	}

	uint8_t code_gen_state::chunk_of(core_ast::node_id n)
	{
		assert(node_to_chunk.find(n) != node_to_chunk.end());
		return node_to_chunk.at(n);
	}

	code_gen_result::code_gen_result() : result_size(0), code_location() {}
	code_gen_result::code_gen_result(uint8_t size, far_lbl loc) 
		: result_size(size), code_location(loc) {}

	void code_gen_state::add_function_loc(std::string name, far_lbl l)
	{
		this->functions.insert({ name, l });
	}
	far_lbl code_gen_state::function_loc(std::string name)
	{
		return this->functions.find(name)->second;
	}
}

namespace fe::vm
{
	code_gen_result generate_bytecode(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i);

	void link_to_parent_chunk(core_ast::node_id n, core_ast::ast& ast, code_gen_state& i)
	{
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
	}

	code_gen_result generate_number(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i) 
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
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::I32: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<int32_t>(num.value)));
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::UI64: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<uint64_t>(num.value)));
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip));
		}
		case number_type::I64: {
			auto location = bc.add_instruction(make_mv(r_result, static_cast<int64_t>(num.value)));
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip));
		}
		default: assert(!"Number type not supported");
		}

	}
	code_gen_result generate_string(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI string");
	}
	code_gen_result generate_boolean(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));

		auto& value = ast.get_data<fe::boolean>(*ast.get_node(n).data_index).value;
		auto r_res = i.alloc_register();
		auto location = bc.add_instruction(make_mv(r_res, value ? 1 : 0));

		return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip));
	}

	code_gen_result generate_function(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto id = p.add_chunk(bytecode());
		auto& func_bc = p.get_chunk(id);
		i.link_node_chunk(n, id);

		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);

		auto used_registers = i.clear_registers();
		for (reg r : used_registers)
			func_bc.add_instruction(make_push64(r));

		// Generate body
		auto i2 = generate_bytecode(node.children[0], ast, p, i);

		for (reg r : used_registers)
			func_bc.add_instruction(make_pop64(r));

		auto& func_data = ast.get_data<core_ast::function_data>(*node.data_index);
		assert(func_data.out_size == i2.result_size);
		p.get_chunk(id).add_instruction(make_ret(func_data.in_size));

		return code_gen_result(0, far_lbl(id, 0));
	}
	code_gen_result generate_tuple(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		link_to_parent_chunk(n, ast, info);

		auto& node = ast.get_node(n);
		uint8_t bytes_sum = 0;
		auto first_info = generate_bytecode(node.children[0], ast, p, info);

		for (auto i = 1; i < node.children.size(); i++)
			bytes_sum += generate_bytecode(node.children[i], ast, p, info).result_size;

		return code_gen_result(bytes_sum, far_lbl(info.chunk_of(n), first_info.code_location.ip));
	}
	code_gen_result generate_block(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
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
			auto r_sink = info.alloc_register();
			auto first_info = generate_bytecode(children[0], ast, p, info);
			for (auto i = 0; i < first_info.result_size; i++)
				p.get_chunk(chunk_id).add_instruction(make_pop8(r_sink));

			// Middle elems
			for (auto i = 1; i < children.size() - 1; i++)
			{
				auto i1 = generate_bytecode(children[i], ast, p, info);

				r_sink = info.alloc_register();
				for (auto j = 0; j < i1.result_size; j++)
					p.get_chunk(chunk_id).add_instruction(make_pop8(r_sink));
			}

			// Last elem
			auto last_info = generate_bytecode(children.back(), ast, p, info);

			return code_gen_result(last_info.result_size, first_info.code_location);
		}
		else
		{
			auto first_info = generate_bytecode(children[0], ast, p, info);
			return code_gen_result(first_info.result_size, first_info.code_location);
		}
	}
	code_gen_result generate_function_call(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));

		far_lbl func_loc = i.function_loc(ast.get_data<core_ast::function_call_data>(*node.data_index).name);

		// Put params on stack
		auto& param_node = ast.get_node(node.children[0]);
		auto i1 = generate_bytecode(param_node.id, ast, p, i);

		// Perform call, putting return address on stack
		auto r_target_bc = i.alloc_register();
		auto r_target_ip = i.alloc_register();
		auto& c = p.get_chunk(i.chunk_of(n));
		c.add_instructions({
			make_mv(r_target_bc, func_loc.chunk_id),
			make_mv(r_target_ip, func_loc.ip),
			make_call(r_target_bc, r_target_ip)
			});

		assert(node.size);
		return code_gen_result(*node.size, far_lbl(i.chunk_of(n), i1.code_location.ip));
	}
	code_gen_result generate_reference(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI ref");
	}
	code_gen_result generate_return(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));

		auto size = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto l = bc.add_instructions({ make_ret(static_cast<uint8_t>(size.val)) });

		return code_gen_result(size.val, far_lbl(i.chunk_of(n), l.ip));
	}
	code_gen_result generate_move(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));

	}
	code_gen_result generate_binary_op(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_chunk(i.chunk_of(n));
		auto& children = ast.children_of(n);

		// children bytecode should leave 2 values on stack
		auto i1 = generate_bytecode(children[0], ast, p, i);
		assert(i1.result_size <= 8);
		auto i2 = generate_bytecode(children[1], ast, p, i);
		assert(i2.result_size <= 8);

		auto r_res_lhs = i.alloc_register();
		auto r_res_rhs = i.alloc_register();
		auto info = bc.add_instructions({
			make_pop(i1.result_size, r_res_rhs),
			make_pop(i2.result_size, r_res_lhs),
			});
		auto r_res = i.alloc_register();

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
		auto size = i1.result_size > i2.result_size
			? i1.result_size
			: i2.result_size;
		bc.add_instruction(make_push(size, r_res));

		return code_gen_result(size, far_lbl(i.chunk_of(n), info.ip));
	}

	code_gen_result generate_bytecode(core_ast::node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::NOP: return code_gen_result();
		case core_ast::node_type::NUMBER: return generate_number(n, ast, p, i);
		case core_ast::node_type::STRING: return generate_string(n, ast, p, i);
		case core_ast::node_type::BOOLEAN: return generate_boolean(n, ast, p, i);
		case core_ast::node_type::FUNCTION: return generate_function(n, ast, p, i);
		case core_ast::node_type::TUPLE: return generate_tuple(n, ast, p, i);
		case core_ast::node_type::BLOCK: return generate_block(n, ast, p, i);
		case core_ast::node_type::FUNCTION_CALL: return generate_function_call(n, ast, p, i);
		case core_ast::node_type::REFERENCE: return generate_reference(n, ast, p, i);
		case core_ast::node_type::RET: return generate_return(n, ast, p, i);
		case core_ast::node_type::MOVE: return generate_move(n, ast, p, i);
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

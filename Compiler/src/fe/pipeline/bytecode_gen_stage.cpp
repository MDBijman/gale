#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/core_stack_analysis.h"

namespace fe::vm
{
	code_gen_state::code_gen_state(core_ast::label first_label) : next_label(first_label) {}

	std::vector<reg> code_gen_state::clear_registers()
	{
		std::vector<reg> out;
		for (auto i = 0; i < 64; i++)
		{
			if (!used_registers.test(i)) continue;

			used_registers.set(i, false);
			out.push_back(i);
		}
		return out;
	}

	void code_gen_state::set_saved_registers(std::vector<reg> regs)
	{
		for (auto r : regs)
		{
			this->used_registers.set(r.val, true);
		}
	}

	reg code_gen_state::alloc_register()
	{
		for (int i = 0; i < 64; i++)
		{
			if (!used_registers.test(i))
			{
				used_registers.set(i);
				return reg(i);
			}
		}
		throw std::runtime_error("ICE: Ran out of registers!");
	}

	void code_gen_state::dealloc_register(reg r)
	{
		this->used_registers.set(r.val, false);
	}

	void code_gen_state::link_node_chunk(node_id n, uint8_t c)
	{
		node_to_chunk.insert_or_assign(n, c);
	}

	uint8_t code_gen_state::chunk_of(node_id n)
	{
		assert(node_to_chunk.find(n) != node_to_chunk.end());
		return node_to_chunk.at(n);
	}

	code_gen_result::code_gen_result() : result_size(0), code_location(), code_size() {}
	code_gen_result::code_gen_result(int64_t size, far_lbl loc, uint32_t code_size)
		: result_size(size), code_location(loc), code_size(code_size) {}

	core_ast::label code_gen_state::get_function_label(const std::string& name)
	{
		auto p = this->functions.find(name);
		if (p == this->functions.end())
		{
			auto new_label = next_label;
			next_label.id++;
			this->functions.insert({ name, new_label });
			return new_label;
		}
		return p->second;
	}
}

namespace fe::vm
{
	// Size of a word on x86 in terms of bytes
	static constexpr int X86_WORD_SIZE = 2;

	code_gen_result generate_bytecode(node_id n, core_ast::ast& ast, program& p, code_gen_state& i);

	void link_to_parent_chunk(node_id n, core_ast::ast& ast, code_gen_state& i)
	{
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
	}

	code_gen_result generate_number(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& num = ast.get_data<fe::number>(*ast.get_node(n).data_index);
		size_t size = 0;
		reg r_result = i.alloc_register();
		switch (num.type)
		{
		case number_type::UI8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui8(r_result, static_cast<uint8_t>(num.value)),
				make_push8(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 1;
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i8(r_result, static_cast<int8_t>(num.value)),
				make_push8(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 1;
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui16(r_result, static_cast<uint16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 2;
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i16(r_result, static_cast<int16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 2;
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui32(r_result, static_cast<uint32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 4;
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i32(r_result, static_cast<int32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 4;
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui64(r_result, static_cast<uint64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 8;
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i64(r_result, static_cast<int64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_register(r_result);
			i.curr_frame_size += 8;
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip), size);
		}
		default: assert(!"Number type not supported");
		}

	}

	code_gen_result generate_string(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI string");
	}

	code_gen_result generate_boolean(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto value = ast.get_data<fe::boolean>(*ast.get_node(n).data_index).value;

		// We cannot push literals onto the stack (yet) so we move it into a register and push the register
		auto r_res = i.alloc_register();
		auto[location, size] = bc.add_instructions(
			make_mv_reg_ui8(r_res, value ? 1 : 0),
			make_push8(r_res)
		);
		i.dealloc_register(r_res);
		i.curr_frame_size += 1;
		return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
	}

	code_gen_result generate_function(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto id = p.add_function(function());
		i.link_node_chunk(n, id);

		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);

		auto stack_analysis = core_ast::analyze_stack(node.id, ast);
		i.analyzed_functions[id] = stack_analysis;

		// Register function
		auto& func_data = ast.get_data<core_ast::function_data>(*node.data_index);
		p.get_function(id).get_name() = func_data.name;

		// We haven't used any registers yet so clear them so they don't get saved
		i.clear_registers();
		auto prev_frame_size = i.curr_frame_size;
		i.curr_frame_size = 0;

		// Generate body
		auto[loc, size] = p.get_function(id).get_bytecode().add_instruction(make_lbl(i.get_function_label(func_data.name).id));

		auto locals_alloc_res = p.get_function(id).get_bytecode().add_instruction(make_salloc_reg_ui8(vm::ret_reg, func_data.locals_size));
		i.curr_frame_size += func_data.locals_size;
		size += locals_alloc_res.second;

		auto body_res = generate_bytecode(node.children[0], ast, p, i);
		size += body_res.code_size;

		// Check bytecode ends with a return, not perfect as the op_to_byte(ret) could be argument of other instruction
		assert(p.get_function(id).get_bytecode().get_instruction<2>(near_lbl(size - op_size(op_kind::RET_UI8)))[0].val == op_to_byte(op_kind::RET_UI8));

		// Locals dealloc
		far_lbl epilogue_padding_loc = far_lbl(id, size - op_size(op_kind::RET_UI8));
		p.insert_padding(epilogue_padding_loc, op_size(op_kind::SDEALLOC_UI8));
		size += op_size(op_kind::SDEALLOC_UI8);

		// Set locals dealloc
		p.get_function(id).get_bytecode().set_instruction(epilogue_padding_loc.ip, make_sdealloc_ui8(func_data.locals_size));

		return code_gen_result(0, far_lbl(id, 0), 0);
	}

	code_gen_result generate_tuple(node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		link_to_parent_chunk(n, ast, info);

		auto& node = ast.get_node(n);
		uint32_t stack_size = 0;
		uint32_t code_size = 0;

		if (node.children.size() == 0)
		{
			return code_gen_result(
				stack_size,
				far_lbl(info.chunk_of(n), p.get_function(info.chunk_of(n)).get_bytecode().data().size()),
				code_size
			);
		}

		auto first_info = generate_bytecode(node.children[0], ast, p, info);
		stack_size += first_info.result_size;
		code_size += first_info.code_size;

		for (auto i = 1; i < node.children.size(); i++)
		{
			auto res = generate_bytecode(node.children[i], ast, p, info);
			stack_size += res.result_size;
			code_size += res.code_size;
		}

		return code_gen_result(stack_size, far_lbl(info.chunk_of(n), first_info.code_location.ip), code_size);
	}

	code_gen_result generate_block(node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		auto chunk_id = 0;
		bool is_root = ast.root_id() == n;

		if (is_root) // Create new chunk
		{
			chunk_id = p.add_function(function{ "_main", bytecode(), {} });
			info.link_node_chunk(n, chunk_id);
		}
		else // Link to parent chunk
		{
			link_to_parent_chunk(n, ast, info);
			chunk_id = info.chunk_of(n);
		}

		auto& children = ast.children_of(n);

		assert(children.size() > 0);

		far_lbl location(chunk_id, p.get_function(chunk_id).get_bytecode().size());

		auto& analysis = info.analyzed_functions[chunk_id];

		int64_t stack_size = *ast.get_node(n).size;
		uint32_t code_size = 0;

		for (auto i = 0; i < children.size(); i++)
		{
			auto child_res = generate_bytecode(children[i], ast, p, info);
			code_size += child_res.code_size;
		}

		if (is_root)
		{
			assert(stack_size >= 0);
			if (stack_size > 0)
			{
				code_size += p.get_function(info.chunk_of(n))
					.get_bytecode()
					.add_instruction(make_sdealloc_ui8(static_cast<uint8_t>(stack_size))).second;
				info.curr_frame_size -= stack_size;
			}
			code_size += p.get_function(info.chunk_of(n)).get_bytecode().add_instruction(make_exit()).second;
		}

		return code_gen_result(stack_size, location, code_size);
	}

	code_gen_result generate_function_call(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		int64_t code_size = 0;

		auto& call_data = ast.get_data<core_ast::function_call_data>(*node.data_index);
		core_ast::label func_label = i.get_function_label(call_data.name);
		p.get_function(i.chunk_of(n)).get_symbols().insert({ func_label.id, call_data.name });

		// Save temporary registers that are not deallocated (e.g. containing parameter locations)
		auto registers_to_save = i.clear_registers();
		for (reg r : registers_to_save)
		{
			code_size += bc.add_instruction(make_push64(r)).second;
			i.curr_frame_size += 8;
		}

		// Put params on stack
		// #todo #fixme this might not work if the params mutate a variable that is stored in a register
		// because the register has already been saved and will be restored with the old contents
		auto& param_node = ast.get_node(node.children[0]);
		auto i1 = generate_bytecode(param_node.id, ast, p, i);
		code_size += i1.code_size;

		// Temporary registers used in parameter code can be safely deallocated
		i.clear_registers();

		// Perform call
		assert(node.size);
		assert(*node.size <= 8);
		code_size += bc.add_instruction(make_call_ui64(func_label.id)).second;

		// Restore temporary registers
		for (auto it = registers_to_save.rbegin(); it != registers_to_save.rend(); it++)
		{
			code_size += bc.add_instruction(make_pop64(it->val)).second;
			i.curr_frame_size -= 8;
		}

		// Set saved register to be as before so they get saved during before next function call
		i.set_saved_registers(registers_to_save);

		if (*node.size > 0)
		{
			// Push result of function call onto stack
			code_size += bc.add_instruction(make_push(*node.size, vm::ret_reg)).second;
			i.curr_frame_size += *node.size;
		}

		return code_gen_result(*node.size, far_lbl(i.chunk_of(n), i1.code_location.ip), code_size);
	}

	code_gen_result generate_reference(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI ref");
	}

	code_gen_result generate_return(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		size_t code_size = 0;

		code_gen_result res = generate_bytecode(node.children[0], ast, p, i);
		code_size += res.code_size;

		if (res.result_size > 0)
		{
			code_size += bc.add_instruction(make_pop(res.result_size, vm::ret_reg)).second;
			i.curr_frame_size -= res.result_size;
		}

		auto in_size = ast.get_node_data<core_ast::return_data>(n);
		code_size += bc.add_instruction(make_ret(static_cast<uint8_t>(in_size.size))).second;

		return code_gen_result(0, far_lbl(i.chunk_of(n), res.code_location.ip), code_size);
	}

	code_gen_result generate_move(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& move_node = ast.get_node(n);
		auto& move_size = ast.get_node_data<core_ast::size>(move_node);
		auto& from = ast.get_node(move_node.children[0]);
		auto& to = ast.get_node(move_node.children[1]);

		near_lbl loc(-1);
		uint32_t stack_size = 0, code_size = 0;

		if (from.kind == core_ast::node_type::VARIABLE && to.kind == core_ast::node_type::STACK_ALLOC)
		{
			assert(move_size.val > 0 && move_size.val <= 16);
			auto v_target = ast.get_node_data<core_ast::var_data>(from);
			auto val_tmp = i.alloc_register();
			auto src_tmp = i.alloc_register();

			auto size = v_target.size;
			i.curr_frame_size += size;
			stack_size = size;
			auto next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));

			auto r = bc.add_instructions(
				make_mv_reg_sp(src_tmp),
				make_add(src_tmp, src_tmp, byte(i.analyzed_functions[i.chunk_of(n)].pre_node_stack_sizes[n] - /*todo skip over params*/ 8 - v_target.offset - next_move_size)),
				make_mv_reg_loc(next_move_size, val_tmp, src_tmp),
				make_push(next_move_size, val_tmp)
			);
			code_size += r.second;

			size -= next_move_size;
			assert(size >= 0);
			next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			while(next_move_size > 0)
			{
				r = bc.add_instructions(
					make_sub(src_tmp, src_tmp, byte(next_move_size)),
					make_mv_reg_loc(next_move_size, val_tmp, src_tmp),
					make_push(next_move_size, val_tmp)
				);
				code_size += r.second;

				size -= next_move_size;
				assert(size >= 0);
				next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			}

			i.dealloc_register(src_tmp);
			i.dealloc_register(val_tmp);
		}
		else if (from.kind == core_ast::node_type::PARAM && to.kind == core_ast::node_type::STACK_ALLOC)
		{
			assert(move_size.val > 0 && move_size.val <= 8);
			auto v_target = ast.get_node_data<core_ast::var_data>(from);
			auto val_tmp = i.alloc_register();
			auto src_tmp = i.alloc_register();
			auto r = bc.add_instructions(
				make_mv_reg_sp(src_tmp),
				make_add(src_tmp, src_tmp, byte(i.analyzed_functions[i.chunk_of(n)].pre_node_stack_sizes[n] - v_target.offset - v_target.size)),
				make_mv_reg_loc(v_target.size, val_tmp, src_tmp),
				make_push(v_target.size, val_tmp)
			);
			i.dealloc_register(src_tmp);
			i.dealloc_register(val_tmp);
			i.curr_frame_size += v_target.size;
			code_size += r.second;
			stack_size = v_target.size;
		}
		else
		{
			throw std::runtime_error("invalid move");
		}

		return code_gen_result(stack_size, far_lbl(i.chunk_of(n), loc.ip), code_size);
	}

	code_gen_result generate_stack_alloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, size] = bc.add_instruction(make_salloc_reg_ui8(reg(vm::ret_reg), data.val));
		i.curr_frame_size += data.val;
		return code_gen_result(data.val, far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_stack_dealloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, size] = bc.add_instruction(make_sdealloc_ui8(data.val));
		i.curr_frame_size -= data.val;
		return code_gen_result(-static_cast<int64_t>(data.val), far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_jump_not_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto test_reg = i.alloc_register();
		auto[loc, size] = bc.add_instructions(
			make_pop8(test_reg),
			// the label is a placeholder for the actual location
			// We must first register all labels before we substitute the locations in the jumps
			// As labels can occur after the jumps to them and locations can still change
			make_jrnz_i32(test_reg, lbl.id)
		);
		i.dealloc_register(test_reg);
		i.curr_frame_size -= 1;
		return { -1, far_lbl(i.chunk_of(n), loc.ip), size };
	}

	code_gen_result generate_jump_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto test_reg = i.alloc_register();
		auto[loc, size] = bc.add_instructions(
			make_pop8(test_reg),
			// the label is a placeholder for the actual location
			// We must first register all labels before we substitute the locations in the jumps
			// As labels can occur after the jumps to them and locations can still change
			make_jrz_i32(test_reg, lbl.id)
		);
		i.dealloc_register(test_reg);
		i.curr_frame_size -= 1;
		return { -1, far_lbl(i.chunk_of(n), loc.ip), size };
	}

	code_gen_result generate_jump(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		// the label is a placeholder for the actual location
		// We must first register all labels before we substitute the locations in the jumps
		// As labels can occur after the jumps to them and locations can still change
		auto[loc, size] = bc.add_instruction(make_jmpr_i32(lbl.id));

		return { 0, far_lbl(i.chunk_of(n), loc.ip), size };
	}

	code_gen_result generate_label(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto[loc, size] = bc.add_instruction(make_lbl(lbl.id));

		return { 0, far_lbl(i.chunk_of(n), loc.ip), size };
	}

	code_gen_result generate_binary_op(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& children = ast.children_of(n);
		int32_t size = 0;

		// children bytecode should leave 2 values on stack
		auto i1 = generate_bytecode(children[0], ast, p, i);
		assert(i1.result_size > 0 && i1.result_size <= 8);
		size += i1.code_size;
		auto i2 = generate_bytecode(children[1], ast, p, i);
		assert(i2.result_size > 0 && i2.result_size <= 8);
		size += i2.code_size;

		auto r_res_lhs = i.alloc_register();
		auto r_res_rhs = i.alloc_register();
		size += bc.add_instructions(
			make_pop(i1.result_size, r_res_rhs),
			make_pop(i2.result_size, r_res_lhs)
		).second;
		i.curr_frame_size -= i1.result_size + i2.result_size;

		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::ADD:size += bc.add_instruction(make_add(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::SUB:size += bc.add_instruction(make_sub(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::MUL:size += bc.add_instruction(make_mul(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::DIV:size += bc.add_instruction(make_div(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::MOD:size += bc.add_instruction(make_mod(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::AND:size += bc.add_instruction(make_and(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::OR: size += bc.add_instruction(make_or(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::LT: size += bc.add_instruction(make_lt(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::LEQ:size += bc.add_instruction(make_lte(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::GT: size += bc.add_instruction(make_gt(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::GEQ:size += bc.add_instruction(make_gte(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::EQ: size += bc.add_instruction(make_eq(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::NEQ:size += bc.add_instruction(make_neq(vm::ret_reg, r_res_lhs, r_res_rhs)).second; break;
		}
		i.dealloc_register(r_res_lhs);
		i.dealloc_register(r_res_rhs);

		uint8_t res_size;
		switch (node.kind)
		{
		case core_ast::node_type::ADD: case core_ast::node_type::SUB:
		case core_ast::node_type::MUL: case core_ast::node_type::DIV:
		case core_ast::node_type::MOD: case core_ast::node_type::AND:
		case core_ast::node_type::OR:
			res_size = i1.result_size > i2.result_size
				? i1.result_size
				: i2.result_size;
			break;
		case core_ast::node_type::LT: case core_ast::node_type::GEQ:
		case core_ast::node_type::GT: case core_ast::node_type::LEQ:
		case core_ast::node_type::EQ: case core_ast::node_type::NEQ:
			res_size = 1;
			break;
		}

		size += bc.add_instruction(make_push(res_size, vm::ret_reg)).second;
		i.curr_frame_size += res_size;

		return code_gen_result(res_size, far_lbl(i.chunk_of(n), i1.code_location.ip), size);
	}

	code_gen_result generate_pop(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& size = ast.get_node_data<core_ast::size>(node);

		int64_t stack_size = 0, code_size = 0;
		near_lbl location = 0;


		auto& target = ast.get_node(node.children[0]);
		switch (target.kind)
		{
		case core_ast::node_type::VARIABLE: {
			assert(size.val > 0 && size.val <= 16);
			auto v_target = ast.get_node_data<core_ast::var_data>(target);
			auto val_tmp = i.alloc_register();
			auto tgt_tmp = i.alloc_register();

			auto size = v_target.size;
			i.curr_frame_size -= size;
			stack_size = size;
			auto next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));

			auto r = bc.add_instructions(
				make_mv_reg_sp(tgt_tmp),
				make_add(tgt_tmp, tgt_tmp, byte(i.analyzed_functions[i.chunk_of(n)].pre_node_stack_sizes[n] 
					- /*todo skip over params*/ 8 - v_target.offset - size)),
				make_pop(next_move_size, val_tmp),
				make_mv_loc_reg(next_move_size, tgt_tmp, val_tmp)
			);
			code_size += r.second;

			size -= next_move_size;
			assert(size >= 0);
			next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			while(next_move_size > 0)
			{
				r = bc.add_instructions(
					make_add(tgt_tmp, tgt_tmp, byte(next_move_size)),
					make_pop(next_move_size, val_tmp),
					make_mv_loc_reg(next_move_size, tgt_tmp, val_tmp)
				);
				code_size += r.second;

				size -= next_move_size;
				assert(size >= 0);
				next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			}

			i.dealloc_register(tgt_tmp);
			i.dealloc_register(val_tmp);
			break;
		}
		case core_ast::node_type::PARAM: {
			assert(size.val <= 8);
			auto v_target = ast.get_node_data<core_ast::var_data>(target);
			auto val_tmp = i.alloc_register();
			auto tgt_tmp = i.alloc_register();
			auto r = bc.add_instructions(
				make_mv_reg_sp(tgt_tmp),
				make_add(tgt_tmp, tgt_tmp, byte(i.analyzed_functions[i.chunk_of(n)].pre_node_stack_sizes[n] - v_target.offset - v_target.size)),
				make_pop(size.val, val_tmp),
				make_mv_loc_reg(size.val, tgt_tmp, val_tmp)
			);
			i.dealloc_register(tgt_tmp);
			i.dealloc_register(val_tmp);
			code_size += r.second;
			i.curr_frame_size -= size.val;
			break;
		}
		default: throw std::runtime_error("unknown pop target");
		}

		return code_gen_result(-static_cast<int64_t>(size.val), far_lbl(i.chunk_of(n), location.ip), code_size);
	}

	code_gen_result generate_bytecode(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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
		case core_ast::node_type::STACK_ALLOC: return generate_stack_alloc(n, ast, p, i);
		case core_ast::node_type::STACK_DEALLOC: return generate_stack_dealloc(n, ast, p, i);
		case core_ast::node_type::JNZ: return generate_jump_not_zero(n, ast, p, i);
		case core_ast::node_type::JZ: return generate_jump_zero(n, ast, p, i);
		case core_ast::node_type::JMP: return generate_jump(n, ast, p, i);
		case core_ast::node_type::LABEL: return generate_label(n, ast, p, i);
		case core_ast::node_type::POP: return generate_pop(n, ast, p, i);
		default:
			if (core_ast::is_binary_op(node.kind)) return generate_binary_op(n, ast, p, i);
			throw std::runtime_error("Unknown node type");
		}
	}

	program generate_bytecode(core_ast::ast& ast)
	{
		// Program that will contain the chunks containing the bytecode
		program p;

		core_ast::ast_helper h(ast);

		// Find the highest label allocated so far in the ast, so function labels wont overlap with jmp labels
		core_ast::label max_lbl{ 0 };
		h.for_all_t(core_ast::node_type::LABEL, [&max_lbl, &ast](core_ast::node& node) {
			auto& data = ast.get_node_data<core_ast::label>(node);
			if (data.id > max_lbl.id) max_lbl.id = data.id + 1;
		});

		// Meta information about intersection between core_ast and bytecode e.g. ast to chunk mapping etc.
		code_gen_state i(max_lbl);

		generate_bytecode(ast.root_id(), ast, p, i);

		return p;
	}
}

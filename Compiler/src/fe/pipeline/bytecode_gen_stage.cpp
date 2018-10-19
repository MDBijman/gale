#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"

namespace fe::vm
{
	std::vector<reg> code_gen_state::clear_temp_registers()
	{
		std::vector<reg> out;
		for (auto i = 0; i < 32; i++)
		{
			if (!used_registers.test(i)) continue;

			used_registers.set(i, false);
			out.push_back(i);
		}
		return out;
	}

	void code_gen_state::set_temp_registers(std::vector<reg> regs)
	{
		for (auto r : regs)
		{
			this->used_registers.set(r.val, true);
		}
	}

	std::vector<reg> code_gen_state::clear_saved_registers()
	{
		// duplicated from clear_temp_registers but with different bounds
		std::vector<reg> out;
		for (auto i = 32; i < 64; i++)
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

	reg code_gen_state::alloc_saved_register()
	{
		for (auto i = 32; i < 64; i++)
		{
			if (!used_registers.test(i))
			{
				used_registers.set(i);
				return reg(i);
			}
		}
		throw std::runtime_error("ICE: Ran out of registers!");
	}

	void code_gen_state::dealloc_saved_register(reg r)
	{
		assert(r.val >= 32 && r.val < 64);
		this->used_registers.set(r.val, false);
	}
	
	reg code_gen_state::alloc_temp_register()
	{
		for (auto i = 0; i < 32; i++)
		{
			if (!used_registers.test(i))
			{
				used_registers.set(i);
				return reg(i);
			}
		}
		throw std::runtime_error("ICE: Ran out of registers!");
	}

	void code_gen_state::dealloc_temp_register(reg r)
	{
		assert(r.val >= 0 && r.val < 32);
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

	core_ast::label code_gen_state::function_label(std::string name)
	{
		auto p = this->functions.find(name);
		if (p == this->functions.end())
		{
			auto lbl = new_function_label();
			this->functions.insert({ name, lbl });
			return lbl;
		}
		else return p->second;
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
		reg r_result = i.alloc_temp_register();
		switch (num.type)
		{
		case number_type::UI8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui8(r_result, static_cast<uint8_t>(num.value)),
				make_push8(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i8(r_result, static_cast<int8_t>(num.value)),
				make_push8(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui16(r_result, static_cast<uint16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i16(r_result, static_cast<int16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui32(r_result, static_cast<uint32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i32(r_result, static_cast<int32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui64(r_result, static_cast<uint64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_temp_register(r_result);
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i64(r_result, static_cast<int64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_temp_register(r_result);
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
		auto r_res = i.alloc_temp_register();
		auto[location, size] = bc.add_instructions(
			make_mv_reg_ui8(r_res, value ? 1 : 0),
			make_push8(r_res)
		);
		i.dealloc_temp_register(r_res);

		return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
	}

	code_gen_result generate_function(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto id = p.add_function(function());
		auto& func = p.get_function(id);
		auto& func_bc = func.get_bytecode();
		i.link_node_chunk(n, id);

		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);

		// Register function
		auto& func_data = ast.get_data<core_ast::function_data>(*node.data_index);
		func.get_name() = func_data.name;

		i.function_label(func_data.name);

		// We haven't used any registers yet so clear them so they don't get saved
		// #todo restore these after generating the function? Only necessary probably if we have functions as values i.e. not top level 
		i.clear_saved_registers();
		i.clear_temp_registers();

		// Allocate the register used for the parameter location
		assert(i.alloc_temp_register().val == 0);

		// Generate body
		auto[loc, size] = func_bc.add_instruction(make_lbl(func_data.label));
		auto body_res = generate_bytecode(node.children[0], ast, p, i);
		size += body_res.code_size;

		/*
		* These registers must be saved on the stack, as they are used in the generated bytecode
		* However, we don't know which registers are used until after bytecode generation
		* So we add the pushes and pops afterwards
		*/
		auto registers_to_save = i.clear_saved_registers();

		// Padding for prologue pushes
		far_lbl prologue_padding_loc = body_res.code_location;
		auto prologue_padding_size = registers_to_save.size() * op_size(op_kind::PUSH64_REG);
		p.insert_padding(prologue_padding_loc, prologue_padding_size);
		size += prologue_padding_size;

		// Padding for epilogue pops
		// Check bytecode ends with a return, not perfect as the op_to_byte(ret) could be argument of other instruction
		assert(func_bc.get_instruction<2>(near_lbl(size - op_size(op_kind::RET_UI8)))[0].val == op_to_byte(op_kind::RET_UI8));
		far_lbl epilogue_padding_loc = far_lbl(id, size - op_size(op_kind::RET_UI8));
		auto epilogue_padding_size = registers_to_save.size() * op_size(op_kind::POP64_REG);
		p.insert_padding(epilogue_padding_loc, epilogue_padding_size);
		size += epilogue_padding_size;

		// Set padding to push and pop
		for (int i = 0; i < registers_to_save.size(); i++)
		{
			func_bc.set_instruction(prologue_padding_loc.ip + (i * 2), make_push64(registers_to_save[i]));
			func_bc.set_instruction(epilogue_padding_loc.ip + (i * 2), make_pop64(registers_to_save[registers_to_save.size() - 1 - i]));
		}

		return code_gen_result(0, far_lbl(id, 0), size);
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

		int64_t stack_size = 0;
		uint32_t code_size = 0;

		for (auto i = 0; i < children.size(); i++)
		{
			auto child_res = generate_bytecode(children[i], ast, p, info);
			code_size += child_res.code_size;
			stack_size += child_res.result_size;
		}

		if (is_root)
		{
			code_size += p.get_function(info.chunk_of(n))
				.get_bytecode()
				.add_instruction(make_sdealloc_ui8(static_cast<uint8_t>(stack_size))).second;
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
		core_ast::label func_label = i.function_label(call_data.name);
		p.get_function(i.chunk_of(n)).get_symbols().insert({ func_label.id, call_data.name });

		// Save temporary registers that are not deallocated (e.g. containing parameter locations)
		auto registers_to_save = i.clear_temp_registers();
		for (reg r : registers_to_save)
		{
			code_size += bc.add_instruction(make_push64(r)).second;
		}

		// Put params on stack
		auto& param_node = ast.get_node(node.children[0]);
		auto i1 = generate_bytecode(param_node.id, ast, p, i);
		code_size += i1.code_size;

		// Temporary registers used in parameter code can be safely deallocated
		// #todo definitely?
		i.clear_temp_registers();

		if (i1.result_size > 8)
		{
			// Repush the params with salloc_reg_ui8 to get stack address into reg(0)
			auto param = i.alloc_temp_register();
			assert(param.val == 0);
			auto buf = i.alloc_temp_register();
			auto i2 = bc.add_instructions(
				make_pop(i1.result_size, buf),
				make_salloc_reg_ui8(param, i1.result_size),
				make_mv_loc_reg(i1.result_size, param, buf)
			);
			code_size += i2.second;

			i.dealloc_temp_register(buf);
		}
		else
		{
			auto i2 = bc.add_instructions(make_pop(i1.result_size, 0));
			code_size += i2.second;
		}

		// Perform call
		assert(node.size);
		assert(*node.size <= 8);
		code_size += bc.add_instruction(make_call_ui64(func_label.id)).second;

		// Restore temporary registers
		for (reg r : registers_to_save)
		{
			code_size += bc.add_instruction(make_pop64(r)).second;
		}

		// Set saved register to be as before so they get saved during before next function call
		i.set_saved_registers(registers_to_save);

		// Push result of function call onto stack
		code_size += bc.add_instruction(make_push(*node.size, vm::ret_reg)).second;

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
		assert(res.result_size == 0);
		code_size += res.code_size;

		auto in_size = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		if (in_size.val > 8)
			code_size += bc.add_instruction(make_ret(static_cast<uint8_t>(in_size.val))).second;
		else
			code_size += bc.add_instruction(make_ret(0)).second;

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

		assert(move_size.val > 0 && move_size.val <= 8);

		if (from.kind == core_ast::node_type::LOCAL_ADDRESS && to.kind == core_ast::node_type::LOCAL_ADDRESS)
		{
			auto r_from = ast.get_node_data<core_ast::size>(from).val, r_to = ast.get_node_data<core_ast::size>(to).val;
			auto r_buf = i.alloc_temp_register();

			switch (move_size.val)
			{
			case 8: code_size += bc.add_instructions(make_mv64_reg_loc(r_buf, r_from), make_mv64_loc_reg(r_to, r_buf)).second; break;
			case 4: code_size += bc.add_instructions(make_mv32_reg_loc(r_buf, r_from), make_mv32_loc_reg(r_to, r_buf)).second; break;
			case 2: code_size += bc.add_instructions(make_mv16_reg_loc(r_buf, r_from), make_mv16_loc_reg(r_to, r_buf)).second; break;
			case 1: code_size += bc.add_instructions(make_mv8_reg_loc(r_buf, r_from), make_mv8_loc_reg(r_to, r_buf)).second; break;
			default: throw std::runtime_error("invalid move size");
			}

			i.dealloc_temp_register(r_buf);
		}
		else if (from.kind == core_ast::node_type::LOCAL_ADDRESS && to.kind == core_ast::node_type::STACK_ALLOC)
		{
			auto r_from = ast.get_node_data<core_ast::size>(from).val;
			auto r_buf = i.alloc_temp_register();

			stack_size = move_size.val;

			switch (move_size.val)
			{
			case 8: code_size += bc.add_instructions(make_mv64_reg_loc(r_buf, r_from), make_push64(r_buf)).second; break;
			case 4: code_size += bc.add_instructions(make_mv32_reg_loc(r_buf, r_from), make_push32(r_buf)).second; break;
			case 2: code_size += bc.add_instructions(make_mv16_reg_loc(r_buf, r_from), make_push16(r_buf)).second; break;
			case 1: code_size += bc.add_instructions(make_mv8_reg_loc(r_buf, r_from), make_push8(r_buf)).second; break;
			default: throw std::runtime_error("invalid move size");
			}

			i.dealloc_temp_register(r_buf);
		}
		else if (from.kind == core_ast::node_type::REGISTER && to.kind == core_ast::node_type::STACK_ALLOC)
		{
			auto r_from = ast.get_node_data<core_ast::size>(from).val;

			stack_size = move_size.val;

			switch (move_size.val)
			{
			case 8: code_size += bc.add_instructions(make_push64(r_from)).second; break;
			case 4: code_size += bc.add_instructions(make_push32(r_from)).second; break;
			case 2: code_size += bc.add_instructions(make_push16(r_from)).second; break;
			case 1: code_size += bc.add_instructions(make_push8(r_from)).second; break;
			default: throw std::runtime_error("invalid move size");
			}
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
		return code_gen_result(data.val, far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_stack_dealloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, size] = bc.add_instruction(make_sdealloc_ui8(data.val));
		return code_gen_result(-static_cast<int64_t>(data.val), far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_jump_not_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto test_reg = i.alloc_temp_register();
		auto[loc, size] = bc.add_instructions(
			make_pop8(test_reg),
			// the label is a placeholder for the actual location
			// We must first register all labels before we substitute the locations in the jumps
			// As labels can occur after the jumps to them and locations can still change
			make_jrnz_i32(test_reg, lbl.id)
		);
		i.dealloc_temp_register(test_reg);

		return { -1, far_lbl(i.chunk_of(n), loc.ip), size };
	}

	code_gen_result generate_jump_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto test_reg = i.alloc_temp_register();
		auto[loc, size] = bc.add_instructions(
			make_pop8(test_reg),
			// the label is a placeholder for the actual location
			// We must first register all labels before we substitute the locations in the jumps
			// As labels can occur after the jumps to them and locations can still change
			make_jrz_i32(vm::ret_reg, lbl.id)
		);
		i.dealloc_temp_register(test_reg);

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

		auto r_res_lhs = i.alloc_temp_register();
		auto r_res_rhs = i.alloc_temp_register();
		size += bc.add_instructions(
			make_pop(i1.result_size, r_res_rhs),
			make_pop(i2.result_size, r_res_lhs)
		).second;

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
		i.dealloc_temp_register(r_res_lhs);
		i.dealloc_temp_register(r_res_rhs);

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
		case core_ast::node_type::RESULT_REGISTER:
			std::tie(location, code_size) = bc.add_instruction(make_pop(size.val, vm::ret_reg)); break;
		default: throw std::runtime_error("unknown pop result");
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

	program generate_bytecode(core_ast::ast& n)
	{
		// Program that will contain the chunks containing the bytecode
		program p;

		// Meta information about intersection between core_ast and bytecode e.g. ast to chunk mapping etc.
		code_gen_state i;

		generate_bytecode(n.root_id(), n, p, i);

		return p;
	}
	}

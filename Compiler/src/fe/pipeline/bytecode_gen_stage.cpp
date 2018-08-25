#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/pipeline/vm_stage.h"
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
		this->in_use.push_back(res);
		this->next_allocation.val++;
		return res;
	}

	reg code_gen_state::alloc_variable_register(uint8_t var)
	{
		if (auto loc = var_to_reg.find(var); loc != var_to_reg.end())
		{
			return loc->second;
		}
		else
		{
			reg r = alloc_register();
			var_to_reg.insert({ var, r.val });
			return r;
		}
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
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i8(r_result, static_cast<int8_t>(num.value)),
				make_push8(r_result)
			);
			return code_gen_result(1, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui16(r_result, static_cast<uint16_t>(num.value)),
				make_push16(r_result)
			);
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i16(r_result, static_cast<int16_t>(num.value)),
				make_push16(r_result)
			);
			return code_gen_result(2, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui32(r_result, static_cast<uint32_t>(num.value)),
				make_push32(r_result)
			);
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i32(r_result, static_cast<int32_t>(num.value)),
				make_push32(r_result)
			);
			return code_gen_result(4, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::UI64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui64(r_result, static_cast<uint64_t>(num.value)),
				make_push64(r_result)
			);
			return code_gen_result(8, far_lbl(i.chunk_of(n), location.ip), size);
		}
		case number_type::I64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i64(r_result, static_cast<int64_t>(num.value)),
				make_push64(r_result)
			);
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
		i.clear_registers();

		// Generate body
		auto[loc, size] = func_bc.add_instruction(make_lbl(func_data.label));
		auto body_res = generate_bytecode(node.children[0], ast, p, i);
		size += body_res.code_size;

		/*
		* These registers must be saved on the stack, as they are used in the generated bytecode
		* However, we don't know which registers are used until after bytecode generation
		* So we add the pushes and pops afterwards
		*/
		auto register_allocations = i.clear_registers();

		// Padding for prologue pushes
		far_lbl prologue_padding_loc = body_res.code_location;
		auto prologue_padding_size = register_allocations.size() * op_size(op_kind::PUSH64_REG);
		p.insert_padding(prologue_padding_loc, prologue_padding_size);
		size += prologue_padding_size;

		// Padding for epilogue pops
		// Check bytecode ends with a return, not perfect as the op_to_byte(ret) could be argument of other instruction
		assert(func_bc.get_instruction<2>(near_lbl(size - op_size(op_kind::RET_UI8)))[0].val == op_to_byte(op_kind::RET_UI8));
		far_lbl epilogue_padding_loc = far_lbl(id, size - op_size(op_kind::RET_UI8));
		auto epilogue_padding_size = register_allocations.size() * op_size(op_kind::POP64_REG);
		p.insert_padding(epilogue_padding_loc, epilogue_padding_size);
		size += epilogue_padding_size;

		// Set padding to push and pop
		for (int i = 0; i < register_allocations.size(); i++)
		{
			func_bc.set_instruction(prologue_padding_loc.ip + (i * 2), make_push64(register_allocations[i]));
			func_bc.set_instruction(epilogue_padding_loc.ip + (i * 2), make_pop64(register_allocations[register_allocations.size() - 1 - i]));
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

		return code_gen_result(stack_size, location, code_size);
	}

	code_gen_result generate_function_call(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& call_data = ast.get_data<core_ast::function_call_data>(*node.data_index);
		core_ast::label func_label = i.function_label(call_data.name);
		p.get_function(i.chunk_of(n)).get_symbols().insert({ func_label.id, call_data.name });

		// Put params on stack
		auto& param_node = ast.get_node(node.children[0]);
		auto i1 = generate_bytecode(param_node.id, ast, p, i);

		// Perform call
		assert(node.size);
		assert(*node.size <= 8);

		auto[loc, size] = bc.add_instructions(
			make_call_ui64(func_label.id),
			make_push64(vm::ret_reg),
			make_sub(vm::sp_reg, vm::sp_reg, byte(8 - *node.size))
		);

		return code_gen_result(*node.size, far_lbl(i.chunk_of(n), i1.code_location.ip), size + i1.code_size);
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
		assert(res.result_size > 0 && res.result_size <= 8);
		code_size += res.code_size;

		int64_t mask = 0;
		while (res.result_size > 0) { res.result_size--; mask >>= 8; mask |= 0xFF00000000000000; }
		auto tmp = i.alloc_register();
		auto[_, size] = bc.add_instructions(
			// Put stack offset in target
			make_mv_reg_i16(vm::ret_reg, -8),
			// Add to this address the stack ptr, giving address
			make_add(reg(vm::ret_reg), reg(vm::sp_reg), reg(vm::ret_reg)),
			// Move 8 bytes at adress into the register
			make_mv64_reg_loc(vm::ret_reg, vm::ret_reg),
			// Zero extra copied bytes (e.g. if we needed 5 bytes we zero (8 - 5 =) 3 bytes)
			make_mv_reg_i64(tmp, mask),
			make_and(vm::ret_reg, vm::ret_reg, reg(tmp))
		);
		code_size += size;

		auto in_size = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		code_size += bc.add_instruction(make_ret(static_cast<uint8_t>(in_size.val))).second;

		return code_gen_result(0, far_lbl(i.chunk_of(n), res.code_location.ip), code_size);
	}

	code_gen_result generate_move(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& move_node = ast.get_node(n);
		auto& move_data = ast.get_data<core_ast::move_data>(*move_node.data_index);
		auto& f_t = move_data.from_t;
		auto& t_t = move_data.to_t;

		near_lbl loc(-1);
		uint32_t size = 0;

		if (t_t == core_ast::move_data::dst_type::REG)
		{ 
			move_data.to = i.alloc_variable_register(move_data.to).val;
		}
		if (f_t == core_ast::move_data::src_type::REG)
		{ 
			move_data.from = i.alloc_variable_register(move_data.from).val;
		}

		if (t_t == core_ast::move_data::dst_type::RES)
		{
			t_t = core_ast::move_data::dst_type::REG;
			move_data.to = vm::ret_reg;
		}

		// Converting special register src types into regular registers
		if (f_t == core_ast::move_data::src_type::SP)
		{
			f_t = core_ast::move_data::src_type::REG;
			move_data.from = vm::sp_reg;
			move_data.size = 8;
		}
		if (f_t == core_ast::move_data::src_type::RES)
		{
			f_t = core_ast::move_data::src_type::REG;
			move_data.from = vm::ret_reg;
			move_data.size = 8;
		}
		if (f_t == core_ast::move_data::src_type::IP)
		{
			f_t = core_ast::move_data::src_type::REG;
			move_data.from = vm::ip_reg;
			move_data.size = 8;
		}


		// #todo compose/simplify/dry further
		if (t_t == core_ast::move_data::dst_type::REG && f_t == core_ast::move_data::src_type::REG)
		{
			std::tie(loc, size) = bc.add_instruction(make_mv_reg_reg(move_data.size, reg(move_data.to), reg(move_data.from)));
		}
		else if (t_t == core_ast::move_data::dst_type::REG && f_t == core_ast::move_data::src_type::LOC)
		{
			std::tie(loc, size) = bc.add_instruction(make_mv_loc_reg(move_data.size, reg(move_data.to), reg(move_data.from)));
		}
		else if (t_t == core_ast::move_data::dst_type::REG && f_t == core_ast::move_data::src_type::STACK)
		{
			assert(move_data.size > 0 && move_data.size <= 8);
			auto size = move_data.size;
			int64_t mask = 0;
			while (size > 0) { size--; mask >>= 8; mask |= 0xFF00000000000000; }
			auto tmp = i.alloc_register();
			std::tie(loc, size) = bc.add_instructions(
				// Put stack offset in target
				make_mv_reg_i16(move_data.to, move_data.from),
				// Add to this address the stack ptr, giving address
				make_add(reg(move_data.to), reg(vm::sp_reg), reg(move_data.to)),
				// Move 8 bytes at adress into the register
				make_mv64_reg_loc(move_data.to, move_data.to),
				// Zero extra copied bytes (e.g. if we needed 5 bytes we zero (8 - 5 = ) 3 bytes)
				make_mv_reg_i64(tmp, mask),
				make_and(move_data.to, move_data.to, reg(tmp))
			);
		}
		else if (t_t == core_ast::move_data::dst_type::LOC && f_t == core_ast::move_data::src_type::STACK)
		{
			auto r_from = i.alloc_register();
			std::tie(loc, size) = bc.add_instructions(
				make_mv_reg_i16(r_from, move_data.from),
				make_add(r_from, vm::sp_reg, r_from)
			);
			auto r_to = move_data.to;
			auto r_buff = i.alloc_register();
			int i = 0;
			for (; (i + 1) * 8 <= move_data.size; i += 8) size += bc.add_instructions(make_mv64_reg_loc(r_buff, r_from), make_mv64_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(8)), make_add(r_to, r_to, byte(8))).second;
			for (; (i + 1) * 4 <= move_data.size; i += 4) size += bc.add_instructions(make_mv32_reg_loc(r_buff, r_from), make_mv32_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(4)), make_add(r_to, r_to, byte(4))).second;
			for (; (i + 1) * 2 <= move_data.size; i += 2) size += bc.add_instructions(make_mv16_reg_loc(r_buff, r_from), make_mv16_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(2)), make_add(r_to, r_to, byte(2))).second;
			for (; (i + 1) <= move_data.size; i += 1) size += bc.add_instructions(make_mv8_reg_loc(r_buff, r_from), make_mv8_loc_reg(r_to, r_buff)).second;
		}
		else if (t_t == core_ast::move_data::dst_type::STACK && f_t == core_ast::move_data::src_type::LOC_WITH_OFFSET)
		{
			auto r_from = i.alloc_variable_register(static_cast<uint32_t>(move_data.from >> 32));
			auto offset = static_cast<uint32_t>(move_data.from & 0xFFFFFFFF);

			std::tie(loc, size) = bc.add_instruction(make_add(r_from, r_from, byte(offset)));
			auto r_to = i.alloc_register();
			size += bc.add_instructions(
				make_mv_reg_i16(r_to, move_data.to),
				make_add(r_to, vm::sp_reg, r_to)
			).second;

			auto r_buff = i.alloc_register();
			int i = 0;
			for (; (i + 1) * 8 <= move_data.size; i += 8) size += bc.add_instructions(make_mv64_reg_loc(r_buff, r_from), make_mv64_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(8)), make_add(r_to, r_to, byte(8))).second;
			for (; (i + 1) * 4 <= move_data.size; i += 4) size += bc.add_instructions(make_mv32_reg_loc(r_buff, r_from), make_mv32_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(4)), make_add(r_to, r_to, byte(4))).second;
			for (; (i + 1) * 2 <= move_data.size; i += 2) size += bc.add_instructions(make_mv16_reg_loc(r_buff, r_from), make_mv16_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(2)), make_add(r_to, r_to, byte(2))).second;
			for (; (i + 1) <= move_data.size; i += 1) size += bc.add_instructions(make_mv8_reg_loc(r_buff, r_from), make_mv8_loc_reg(r_to, r_buff)).second;
		}
		else if (t_t == core_ast::move_data::dst_type::STACK && f_t == core_ast::move_data::src_type::STACK)
		{
			auto r_from = i.alloc_register();
			std::tie(loc, size) = bc.add_instructions(
				make_mv_reg_i16(r_from, move_data.from),
				make_add(r_from, vm::sp_reg, r_from)
			);
			auto r_to = i.alloc_register();
			size += bc.add_instructions(
				make_mv_reg_i16(r_to, move_data.to),
				make_add(r_to, vm::sp_reg, r_to)
			).second;
			auto r_buff = i.alloc_register();
			int i = 0;
			for (; (i + 1) * 8 <= move_data.size; i += 8) size += bc.add_instructions(make_mv64_reg_loc(r_buff, r_from), make_mv64_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(8)), make_add(r_to, r_to, byte(8))).second;
			for (; (i + 1) * 4 <= move_data.size; i += 4) size += bc.add_instructions(make_mv32_reg_loc(r_buff, r_from), make_mv32_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(4)), make_add(r_to, r_to, byte(4))).second;
			for (; (i + 1) * 2 <= move_data.size; i += 2) size += bc.add_instructions(make_mv16_reg_loc(r_buff, r_from), make_mv16_loc_reg(r_to, r_buff), make_add(r_from, r_from, byte(2)), make_add(r_to, r_to, byte(2))).second;
			for (; (i + 1) <= move_data.size; i += 1) size += bc.add_instructions(make_mv8_reg_loc(r_buff, r_from), make_mv8_loc_reg(r_to, r_buff)).second;
		}
		else { assert(!"nyi"); }

		return code_gen_result(0, far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_stack_alloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, size] = bc.add_instruction(make_add(reg(vm::sp_reg), reg(vm::sp_reg), byte(data.val)));
		return code_gen_result(data.val, far_lbl(i.chunk_of(n), loc.ip), size);
	}

	code_gen_result generate_stack_dealloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, size] = bc.add_instruction(make_sub(reg(vm::sp_reg), reg(vm::sp_reg), byte(data.val)));
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
		assert(i1.result_size <= 8);
		size += i1.code_size;
		auto i2 = generate_bytecode(children[1], ast, p, i);
		assert(i2.result_size <= 8);
		size += i2.code_size;

		auto r_res_lhs = i.alloc_register();
		auto r_res_rhs = i.alloc_register();
		size += bc.add_instructions(
			make_pop(i1.result_size, r_res_rhs),
			make_pop(i2.result_size, r_res_lhs)
		).second;
		auto r_res = i.alloc_register();

		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::ADD:size += bc.add_instruction(make_add(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::SUB:size += bc.add_instruction(make_sub(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::MUL:size += bc.add_instruction(make_mul(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::DIV:size += bc.add_instruction(make_div(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::MOD:size += bc.add_instruction(make_mod(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::AND:size += bc.add_instruction(make_and(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::OR: size += bc.add_instruction(make_or(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::LT: size += bc.add_instruction(make_gte(r_res, r_res_rhs, r_res_lhs)).second; break;
		case core_ast::node_type::GEQ:size += bc.add_instruction(make_gte(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::GT: size += bc.add_instruction(make_gt(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::LEQ:size += bc.add_instruction(make_gt(r_res, r_res_rhs, r_res_lhs)).second; break;
		case core_ast::node_type::EQ: size += bc.add_instruction(make_eq(r_res, r_res_lhs, r_res_rhs)).second; break;
		case core_ast::node_type::NEQ:size += bc.add_instruction(make_neq(r_res, r_res_lhs, r_res_rhs)).second; break;
		}

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
		size += bc.add_instruction(make_push(res_size, r_res)).second;

		return code_gen_result(res_size, far_lbl(i.chunk_of(n), i1.code_location.ip), size);
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
		case core_ast::node_type::SALLOC: return generate_stack_alloc(n, ast, p, i);
		case core_ast::node_type::SDEALLOC: return generate_stack_dealloc(n, ast, p, i);
		case core_ast::node_type::JNZ: return generate_jump_not_zero(n, ast, p, i);
		case core_ast::node_type::JMP: return generate_jump(n, ast, p, i);
		case core_ast::node_type::LABEL: return generate_label(n, ast, p, i);
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

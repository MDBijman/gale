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

	code_gen_result::code_gen_result() {}

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
	uint32_t code_gen_state::node_pre_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return analyzed_functions.at(function_id).pre_node_stack_sizes.at(node_id);
	}
	uint32_t code_gen_state::node_post_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return analyzed_functions.at(function_id).node_stack_sizes.at(node_id);
	}
	uint32_t code_gen_state::node_diff_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return node_post_stack_size(function_id, node_id) - node_pre_stack_size(function_id, node_id);
	}
}

namespace fe::vm
{

	constexpr size_t RETURN_ADDRESS_SIZE = 8;
}

namespace fe::vm
{
	void generate_bytecode(node_id n, core_ast::ast& ast, program& p, code_gen_state& i);

	void link_to_parent_chunk(node_id n, core_ast::ast& ast, code_gen_state& i)
	{
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
	}

	void generate_number(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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
			break;
		}
		case number_type::I8: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i8(r_result, static_cast<int8_t>(num.value)),
				make_push8(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::UI16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui16(r_result, static_cast<uint16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::I16: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i16(r_result, static_cast<int16_t>(num.value)),
				make_push16(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::UI32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui32(r_result, static_cast<uint32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::I32: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i32(r_result, static_cast<int32_t>(num.value)),
				make_push32(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::UI64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_ui64(r_result, static_cast<uint64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		case number_type::I64: {
			auto[location, size] = bc.add_instructions(
				make_mv_reg_i64(r_result, static_cast<int64_t>(num.value)),
				make_push64(r_result)
			);
			i.dealloc_register(r_result);
			break;
		}
		default: assert(!"Number type not supported");
		}

	}

	void generate_string(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI string");
	}

	void generate_boolean(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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
	}

	void generate_function(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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

		// Generate body
		auto[loc, _] = p.get_function(id).get_bytecode().add_instruction(make_lbl(i.get_function_label(func_data.name).id));

		auto locals_alloc_res = p.get_function(id).get_bytecode().add_instruction(make_salloc_reg_ui8(vm::ret_reg, func_data.locals_size));

		generate_bytecode(node.children[0], ast, p, i);

		// Locals dealloc
		far_lbl epilogue_padding_loc = far_lbl(id, p.get_function(id).get_bytecode().size() - op_size(op_kind::RET_UI8));
		p.insert_padding(epilogue_padding_loc, op_size(op_kind::SDEALLOC_UI8));

		// Set locals dealloc
		p.get_function(id).get_bytecode().set_instruction(epilogue_padding_loc.ip, make_sdealloc_ui8(func_data.locals_size));
	}

	void generate_tuple(node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
	{
		link_to_parent_chunk(n, ast, info);

		auto& node = ast.get_node(n);

		if (node.children.size() == 0)
			return;

		generate_bytecode(node.children[0], ast, p, info);

		for (auto i = 1; i < node.children.size(); i++)
			generate_bytecode(node.children[i], ast, p, info);
	}

	void generate_block(node_id n, core_ast::ast& ast, program& p, code_gen_state& info)
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

		for (auto i = 0; i < children.size(); i++)
			generate_bytecode(children[i], ast, p, info);

		if (is_root)
		{
			int64_t stack_size = *ast.get_node(n).size;
			assert(stack_size >= 0);
			if (stack_size > 0)
			{
				p.get_function(info.chunk_of(n))
					.get_bytecode()
					.add_instruction(make_sdealloc_ui8(static_cast<uint8_t>(stack_size)));
			}
			p.get_function(info.chunk_of(n)).get_bytecode().add_instruction(make_exit());
		}
	}

	void generate_function_call(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto& call_data = ast.get_data<core_ast::function_call_data>(*node.data_index);
		core_ast::label func_label = i.get_function_label(call_data.name);
		p.get_function(i.chunk_of(n)).get_symbols().insert({ func_label.id, call_data.name });

		// Save temporary registers that are not deallocated (e.g. containing parameter locations)
		auto registers_to_save = i.clear_registers();
		for (reg r : registers_to_save)
		{
			bc.add_instruction(make_push64(r));
		}

		// Put params on stack
		// #todo #fixme this might not work if the params mutate a variable that is stored in a register
		// because the register has already been saved and will be restored with the old contents
		for (auto& child : node.children)
			generate_bytecode(child, ast, p, i);

		// Temporary registers used in parameter code can be safely deallocated
		i.clear_registers();

		// Perform call
		assert(node.size);
		bc.add_instruction(make_call_ui64(func_label.id));

		// Restore temporary registers
		assert(registers_to_save.size() == 0); // temp
		for (auto it = registers_to_save.rbegin(); it != registers_to_save.rend(); it++)
		{
			bc.add_instruction(make_pop64(it->val));
		}

		// Set saved register to be as before so they get saved during before next function call
		i.set_saved_registers(registers_to_save);

		if (*node.size > 0 && *node.size <= 8)
		{
			// Push result of function call onto stack
			bc.add_instruction(make_push(*node.size, vm::ret_reg));
		}
	}

	void generate_reference(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		throw std::runtime_error("NYI ref");
	}

	void generate_return(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& node = ast.get_node(n);
		assert(node.children.size() == 1);
		auto f_id = i.chunk_of(n);
		auto& bc = p.get_function(f_id).get_bytecode();

		generate_bytecode(node.children[0], ast, p, i);

		auto diff = i.node_diff_stack_size(f_id, node.children[0]);

		if (diff > 0)
			bc.add_instruction(make_pop(diff, vm::ret_reg));

		auto in_size = ast.get_node_data<core_ast::return_data>(n);
		bc.add_instruction(make_ret(static_cast<uint8_t>(in_size.size)));
	}

	void generate_move(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto& bc = p.get_function(f_id).get_bytecode();
		auto& move_node = ast.get_node(n);
		auto& move_size = ast.get_node_data<core_ast::size>(move_node);
		auto& from = ast.get_node(move_node.children[0]);
		link_to_parent_chunk(from.id, ast, i);
		auto& to = ast.get_node(move_node.children[1]);
		link_to_parent_chunk(to.id, ast, i);

		near_lbl loc(-1);

		if ((from.kind == core_ast::node_type::VARIABLE || from.kind == core_ast::node_type::DYNAMIC_VARIABLE
			|| from.kind == core_ast::node_type::PARAM || from.kind == core_ast::node_type::STACK_DATA)
			&& to.kind == core_ast::node_type::STACK_ALLOC)
		{
			auto v_target = ast.get_node_data<core_ast::var_data>(from);
			auto val_tmp = i.alloc_register();
			auto src_tmp = i.alloc_register();

			auto size = static_cast<uint32_t>(move_size.val);
			auto next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));

			bc.add_instruction(
				make_mv_reg_sp(src_tmp)
			);

			if (from.kind == core_ast::node_type::DYNAMIC_VARIABLE)
			{
				auto offset_reg = i.alloc_register();

				bc.add_instructions(
					make_pop64(offset_reg),
					make_sub(src_tmp, src_tmp, offset_reg)
				);

				i.dealloc_register(offset_reg);
			}

			auto total_frame_size = i.node_pre_stack_size(i.chunk_of(n), n);

			// Variables are placed on the stack after the return address
			byte stack_offset = from.kind == core_ast::node_type::VARIABLE || from.kind == core_ast::node_type::DYNAMIC_VARIABLE
				? byte(total_frame_size - i.current_scope.in_size - RETURN_ADDRESS_SIZE - v_target.offset - next_move_size)
				: byte(total_frame_size - v_target.offset - next_move_size);

			bc.add_instructions(
				make_add(src_tmp, src_tmp, stack_offset),
				make_mv_reg_loc(next_move_size, val_tmp, src_tmp),
				make_push(next_move_size, val_tmp)
			);

			size -= next_move_size;
			assert(size >= 0);
			auto last_move_size = next_move_size;
			next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			while (next_move_size > 0)
			{
				bc.add_instructions(
					make_sub(src_tmp, src_tmp, byte(last_move_size)),
					make_mv_reg_loc(next_move_size, val_tmp, src_tmp),
					make_push(next_move_size, val_tmp)
				);

				size -= next_move_size;
				assert(size >= 0);
				last_move_size = next_move_size;
				next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			}

			i.dealloc_register(src_tmp);
			i.dealloc_register(val_tmp);
		}
		else
		{
			throw std::runtime_error("invalid move");
		}
	}

	void generate_stack_alloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, _] = bc.add_instruction(make_salloc_reg_ui8(reg(vm::ret_reg), data.val));
	}

	void generate_stack_dealloc(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& data = ast.get_data<core_ast::size>(*ast.get_node(n).data_index);
		auto[loc, _] = bc.add_instruction(make_sdealloc_ui8(data.val));
	}

	void generate_jump_not_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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
	}

	void generate_jump_zero(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
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
	}

	void generate_jump(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		// the label is a placeholder for the actual location
		// We must first register all labels before we substitute the locations in the jumps
		// As labels can occur after the jumps to them and locations can still change
		auto[loc, size] = bc.add_instruction(make_jmpr_i32(lbl.id));
	}

	void generate_label(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto[loc, size] = bc.add_instruction(make_lbl(lbl.id));
	}

	void generate_binary_op(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto& bc = p.get_function(f_id).get_bytecode();
		auto& children = ast.children_of(n);

		// children bytecode should leave 2 values on stack
		generate_bytecode(children[0], ast, p, i);
		auto first_size = i.node_diff_stack_size(f_id, children[0]);
		assert(first_size > 0 && first_size <= 8);

		generate_bytecode(children[1], ast, p, i);
		auto second_size = i.node_diff_stack_size(f_id, children[1]);
		assert(second_size > 0 && second_size <= 8);

		auto r_res_lhs = i.alloc_register();
		auto r_res_rhs = i.alloc_register();
		bc.add_instructions(
			make_pop(first_size, r_res_rhs),
			make_pop(second_size, r_res_lhs)
		);

		auto& node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::ADD:bc.add_instruction(make_add(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::SUB:bc.add_instruction(make_sub(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::MUL:bc.add_instruction(make_mul(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::DIV:bc.add_instruction(make_div(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::MOD:bc.add_instruction(make_mod(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::AND:bc.add_instruction(make_and(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::OR: bc.add_instruction(make_or(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::LT: bc.add_instruction(make_lt(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::LEQ:bc.add_instruction(make_lte(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::GT: bc.add_instruction(make_gt(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::GEQ:bc.add_instruction(make_gte(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::EQ: bc.add_instruction(make_eq(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
		case core_ast::node_type::NEQ:bc.add_instruction(make_neq(vm::ret_reg, r_res_lhs, r_res_rhs)); break;
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
			res_size = std::max(first_size, second_size);
			break;
		case core_ast::node_type::LT: case core_ast::node_type::GEQ:
		case core_ast::node_type::GT: case core_ast::node_type::LEQ:
		case core_ast::node_type::EQ: case core_ast::node_type::NEQ:
			res_size = 1;
			break;
		}
		bc.add_instruction(make_push(res_size, vm::ret_reg));
	}

	void generate_pop(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		link_to_parent_chunk(n, ast, i);
		auto& bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto& node = ast.get_node(n);
		auto& data_size = ast.get_node_data<core_ast::size>(node);

		near_lbl location = 0;

		auto& target = ast.get_node(node.children[0]);
		switch (target.kind)
		{
		case core_ast::node_type::VARIABLE:
		case core_ast::node_type::PARAM: {
			auto v_target = ast.get_node_data<core_ast::var_data>(target);
			auto val_tmp = i.alloc_register();
			auto tgt_tmp = i.alloc_register();

			auto size = static_cast<uint32_t>(data_size.val);
			auto next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));

			byte stack_offset = target.kind == core_ast::node_type::VARIABLE || target.kind == core_ast::node_type::DYNAMIC_VARIABLE
				? byte(i.node_pre_stack_size(i.chunk_of(n), n) - i.current_scope.in_size - RETURN_ADDRESS_SIZE - v_target.offset - size)
				: byte(i.node_pre_stack_size(i.chunk_of(n), n) - v_target.offset - size);

			auto r = bc.add_instructions(
				make_mv_reg_sp(tgt_tmp),
				make_add(tgt_tmp, tgt_tmp, stack_offset),
				make_pop(next_move_size, val_tmp),
				make_mv_loc_reg(next_move_size, tgt_tmp, val_tmp)
			);

			size -= next_move_size;
			assert(size >= 0);
			auto last_move_size = next_move_size;
			next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			while (next_move_size > 0)
			{
				r = bc.add_instructions(
					make_add(tgt_tmp, tgt_tmp, byte(last_move_size)),
					make_pop(next_move_size, val_tmp),
					make_mv_loc_reg(next_move_size, tgt_tmp, val_tmp)
				);

				size -= next_move_size;
				assert(size >= 0);
				last_move_size = next_move_size;
				next_move_size = std::clamp(size, uint32_t(0), uint32_t(8));
			}

			i.dealloc_register(tgt_tmp);
			i.dealloc_register(val_tmp);
			break;
		}
		default: throw std::runtime_error("unknown pop target");
		}
	}

	void generate_bytecode(node_id n, core_ast::ast& ast, program& p, code_gen_state& i)
	{
		auto& node = ast.get_node(n);
		switch (node.kind)
		{
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
		case core_ast::node_type::NOP: return;
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
			if (data.id > max_lbl.id) max_lbl.id = data.id;
		});
		max_lbl.id++;

		// Meta information about intersection between core_ast and bytecode e.g. ast to chunk mapping etc.
		code_gen_state i(max_lbl);

		generate_bytecode(ast.root_id(), ast, p, i);

		return p;
	}
}

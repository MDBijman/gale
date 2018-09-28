#include "fe/pipeline/bytecode_optimization_stage.h"
#include "fe/data/bytecode.h"

namespace fe::vm
{
	void remove_nops(executable& e, optimization_settings s);
	void remove_redundant_push_pop(executable& e, optimization_settings s);

	void optimize(executable& e, optimization_settings s)
	{
		remove_redundant_push_pop(e, s);
		remove_nops(e, s);
	}

	/*
	* Codegen generates several situations where a register is pushed and popped immediately
	* This pass converts this into a mov instruction (or just nops if the src and dest are the same)
	*/
	void remove_redundant_push_pop(executable& e, optimization_settings s)
	{
		for (auto i = 0; i < e.code.data().size();)
		{
			auto current_instruction = e.get_instruction<2>(i);
			op_kind kind = byte_to_op(current_instruction[0].val);
			size_t size = op_size(kind);

			auto next_instruction = e.get_instruction<2>(i + size);
			op_kind next_kind = byte_to_op(next_instruction[0].val);
			size_t next_size = op_size(kind);

			switch (kind)
			{
			case op_kind::PUSH64_REG: {
				if (next_kind != op_kind::POP64_REG) break;
				auto first_reg = current_instruction[1].val;
				auto second_reg = next_instruction[1].val;
				if (first_reg == second_reg)
				{
					e.code.data()[i] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 1] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 2] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				else
				{
					*(reinterpret_cast<bytes<3>*>(&e.code.data()[i])) = make_mv64_reg_reg(second_reg, first_reg);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				i += 4;
				continue;
				break;
			}

			case op_kind::PUSH32_REG: {
				if (next_kind != op_kind::POP32_REG) break;
				auto first_reg = current_instruction[1].val;
				auto second_reg = next_instruction[1].val;
				if (first_reg == second_reg)
				{
					e.code.data()[i] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 1] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 2] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				else
				{
					*(reinterpret_cast<bytes<3>*>(&e.code.data()[i])) = make_mv32_reg_reg(second_reg, first_reg);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				i += 4;
				continue;
				break;
			}

			case op_kind::PUSH16_REG: {
				if (next_kind != op_kind::POP16_REG) break;
				auto first_reg = current_instruction[1].val;
				auto second_reg = next_instruction[1].val;
				if (first_reg == second_reg)
				{
					e.code.data()[i] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 1] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 2] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				else
				{
					*(reinterpret_cast<bytes<3>*>(&e.code.data()[i])) = make_mv16_reg_reg(second_reg, first_reg);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				i += 4;
				continue;
				break;
			}

			case op_kind::PUSH8_REG: {
				if (next_kind != op_kind::POP8_REG) break;
				auto first_reg = current_instruction[1].val;
				auto second_reg = next_instruction[1].val;
				if (first_reg == second_reg)
				{
					e.code.data()[i] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 1] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 2] = op_to_byte(op_kind::NOP);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				else
				{
					*(reinterpret_cast<bytes<3>*>(&e.code.data()[i])) = make_mv8_reg_reg(second_reg, first_reg);
					e.code.data()[i + 3] = op_to_byte(op_kind::NOP);
				}
				i += 4;
				continue;
				break;
			}
			}

			i += size;
		}
	}

	void remove_nops(executable& e, optimization_settings s)
	{
		auto nops_between = [](executable& e, int a, int b) {
			int min = a > b ? b : a;
			int max = b > a ? b : a;
			int count = 0;
			for (auto i = min; i < max;)
			{
				op_kind kind = byte_to_op(e.code.data()[i].val);
				if (kind == op_kind::NOP) count++;
				i += op_size(kind);
			}
			return count;
		};

		// Fix jumps
		for (auto i = 0, nop_count = 0; i < e.code.data().size();)
		{
			auto current_op = e.get_instruction<1>(i);
			op_kind kind = byte_to_op(current_op[0].val);
			size_t size = op_size(kind);

			switch (kind)
			{
			case op_kind::NOP: nop_count++; break;
			case op_kind::JMPR_I32: {
				byte* jump_bytes = &e.code.data()[i + 1];
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset + nops_between(e, i, i + jump_offset));
				else
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset - nops_between(e, i, i + jump_offset));
				break;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32: {
				byte* jump_bytes = &e.code.data()[i + 2];
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset + nops_between(e, i, i + jump_offset));
				else
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset - nops_between(e, i, i + jump_offset));
				break;
			}
			case op_kind::CALL_UI64: {
				byte* address_bytes = &e.code.data()[i + 1];
				int64_t jump_offset = read_i64(&address_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<8>*>(address_bytes) = make_ui64(jump_offset + nops_between(e, i, i + jump_offset));
				else
					*reinterpret_cast<bytes<8>*>(address_bytes) = make_ui64(jump_offset - nops_between(e, i, i + jump_offset));
				break;
			}
			}

			i += size;
		}

		// Remove all nops
		for (auto i = 0, nops_passed = 0; i < e.code.data().size();)
		{
			auto current_op = e.get_instruction<1>(i);
			op_kind kind = byte_to_op(current_op[0].val);
			size_t size = op_size(kind);

			switch (kind)
			{
			case op_kind::NOP: nops_passed++; break;
			default:
				if (nops_passed > 0)
				{
					// Move current op back as many places as ops we passed
					for (int j = 0; j < size; j++)
					{
						e.code.data()[(i + j) - nops_passed] = e.code.data()[(i + j)];
						e.code.data()[i + j] = 0;
					}
				}
				break;
			}

			i += size;
		}
	}
}
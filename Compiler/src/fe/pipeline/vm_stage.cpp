#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>

namespace fe::vm
{
	void machine_state::push8(uint8_t b)
	{
		assert(stack_ptr < stack_size);
		stack[stack_ptr++] = b;
	}
	void machine_state::push16(uint16_t b)
	{
		assert(stack_ptr < stack_size - 1);
		*(uint16_t*)(&stack[stack_ptr]) = b;
		stack_ptr += 2;
	}
	void machine_state::push32(uint32_t b)
	{
		assert(stack_ptr < stack_size - 3);
		*(uint32_t*)(&stack[stack_ptr]) = b;
		stack_ptr += 4;
	}
	void machine_state::push64(uint64_t b)
	{
		assert(stack_ptr < stack_size - 7);
		*(uint64_t*)(&stack[stack_ptr]) = b;
		stack_ptr += 8;
	}
	uint8_t machine_state::pop8()
	{
		assert(stack_ptr > 0);
		stack_ptr--;
		return stack[stack_ptr];
	}
	uint16_t machine_state::pop16()
	{
		assert(stack_ptr > 1);
		stack_ptr -= 2;
		return *(uint16_t*)(&stack[stack_ptr]);
	}
	uint32_t machine_state::pop32()
	{
		assert(stack_ptr > 3);
		stack_ptr -= 4;
		return *(uint32_t*)(&stack[stack_ptr]);
	}
	uint64_t machine_state::pop64()
	{
		assert(stack_ptr > 7);
		stack_ptr -= 8;
		return *(uint64_t*)(&stack[stack_ptr]);
	}

	machine_state interpret(program& p)
	{
		machine_state state;

		auto running = true;
		while (p.get_chunk(state.chunk_id).has_instruction(state.instruction_ptr))
		{
			quad_byte qb = p.get_chunk(state.chunk_id).get_instruction(state.instruction_ptr);
			switch (byte_to_op(qb[0].val))
			{
				// Arithmetic

			case op_kind::ADD_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] + state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;
			case op_kind::SUB_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] - state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;
			case op_kind::MUL_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] * state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;
			case op_kind::MOD_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] % state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;

				// Logic

			case op_kind::GT_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] > state.registers[qb[3].val] ? 1 : 0;
				state.instruction_ptr += 4;
				break;
			case op_kind::GTE_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] >= state.registers[qb[3].val] ? 1 : 0;
				state.instruction_ptr += 4;
				break;
			case op_kind::EQ_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] == state.registers[qb[3].val] ? 1 : 0;
				state.instruction_ptr += 4;
				break;
			case op_kind::NEQ_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] != state.registers[qb[3].val] ? 1 : 0;
				state.instruction_ptr += 4;
				break;
			case op_kind::AND_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] & state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;
			case op_kind::OR_REG_REG_REG:
				state.registers[qb[1].val] = state.registers[qb[2].val] | state.registers[qb[3].val];
				state.instruction_ptr += 4;
				break;

				// Control
			case op_kind::LOAD64_REG_REG:
				state.registers[qb[1].val] = *(int64_t*)(&state.stack[state.registers[qb[2].val]]);
				state.instruction_ptr += 3;
				break;
			case op_kind::LOAD_SP_REG:
				state.registers[qb[1].val] = state.stack_ptr;
				state.instruction_ptr += 2;
				break;
			case op_kind::MV_REG_UI8:
				state.registers[qb[1].val] = qb[2].val;
				state.instruction_ptr += 3;
				break;
			case op_kind::PUSH8_REG:
				state.push8(static_cast<uint8_t>(state.registers[qb[1].val]));
				state.instruction_ptr += 2;
				break;
			case op_kind::PUSH16_REG:
				state.push16(static_cast<uint16_t>(state.registers[qb[1].val]));
				state.instruction_ptr += 2;
				break;
			case op_kind::PUSH32_REG:
				state.push32(static_cast<uint32_t>(state.registers[qb[1].val]));
				state.instruction_ptr += 2;
				break;
			case op_kind::PUSH64_REG:
				state.push64(static_cast<uint64_t>(state.registers[qb[1].val]));
				state.instruction_ptr += 2;
				break;
			case op_kind::POP8_REG:
				state.registers[qb[1].val] = state.pop8();
				state.instruction_ptr += 2;
				break;
			case op_kind::POP16_REG:
				state.registers[qb[1].val] = state.pop16();
				state.instruction_ptr += 2;
				break;
			case op_kind::POP32_REG:
				state.registers[qb[1].val] = state.pop32();
				state.instruction_ptr += 2;
				break;
			case op_kind::POP64_REG:
				state.registers[qb[1].val] = state.pop64();
				state.instruction_ptr += 2;
				break;
			case op_kind::JMP_REG: 
				state.instruction_ptr = state.registers[qb[1].val];
				break;
			case op_kind::JNZ_REG_REG: 
				state.instruction_ptr =
					state.registers[qb[1].val] != 0
					? state.registers[qb[2].val]
					: state.instruction_ptr + 3;
				break;
			case op_kind::JZ_REG_REG:
				state.instruction_ptr =
					state.registers[qb[1].val] == 0
					? state.registers[qb[2].val]
					: state.instruction_ptr + 3;
				break;
			case op_kind::CALL_REG_REG:
				state.push8(state.chunk_id);
				state.push8(state.instruction_ptr + 3);
				state.chunk_id = state.registers[qb[1].val];
				state.instruction_ptr = state.registers[qb[2].val];
				break;
			case op_kind::RET_UI8_UI8: {
				auto to_pop = qb[1].val;
				auto return_size = qb[2].val;

				auto after_ip = state.stack[state.stack_ptr - 1 - return_size];
				auto after_c = state.stack[state.stack_ptr - 2 - return_size];

				for (int i = 0; i < return_size; i++)
					state.stack[state.stack_ptr - 2 - to_pop - return_size + i] = state.stack[state.stack_ptr - return_size + i];

				state.instruction_ptr = after_ip;
				state.chunk_id = after_c;

				for (int i = 0; i < 2 + to_pop; i++) state.pop8();

				break;
			}

			case op_kind::NOP: case op_kind::LBL: state.instruction_ptr++;  break;

			default:
				throw std::runtime_error("Unknown instruction");
			}
		}

		return state;
	}
}
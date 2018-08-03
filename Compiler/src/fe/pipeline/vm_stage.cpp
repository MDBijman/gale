#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <string>

namespace fe::vm
{
#define IP registers[ip_reg]
#define SP registers[sp_reg]
	void machine_state::push8(uint8_t b)
	{
		assert(SP < stack_size);
		stack[SP] = b;
		SP++;
	}
	void machine_state::push16(uint16_t b)
	{
		assert(SP < stack_size - 1);
		stack[SP]     = static_cast<uint8_t>((b & 0xFF00) >> 8);
		stack[SP + 1] = static_cast<uint8_t>(b & 0xFF);
		SP += 2;
	}
	void machine_state::push32(uint32_t b)
	{
		assert(SP < stack_size - 3);
		stack[SP]     = static_cast<uint8_t>((b & 0xFF000000) >> 24);
		stack[SP + 1] = static_cast<uint8_t>((b & 0xFF0000) >> 16);
		stack[SP + 2] = static_cast<uint8_t>((b & 0xFF00) >> 8);
		stack[SP + 3] = static_cast<uint8_t>(b & 0xFF);
		SP += 4;
	}
	void machine_state::push64(uint64_t b)
	{
		assert(SP < stack_size - 7);
		stack[SP]     = static_cast<uint8_t>((b & 0xFF00000000000000) >> 56);
		stack[SP + 1] = static_cast<uint8_t>((b & 0xFF000000000000) >> 48);
		stack[SP + 2] = static_cast<uint8_t>((b & 0xFF0000000000) >> 40);
		stack[SP + 3] = static_cast<uint8_t>((b & 0xFF00000000) >> 32);
		stack[SP + 4] = static_cast<uint8_t>((b & 0xFF000000) >> 24);
		stack[SP + 5] = static_cast<uint8_t>((b & 0xFF0000) >> 16);
		stack[SP + 6] = static_cast<uint8_t>((b & 0xFF00) >> 8);
		stack[SP + 7] = static_cast<uint8_t>(b & 0xFF);
		SP += 8;
	}
	uint8_t machine_state::pop8()
	{
		assert(SP > 0);
		SP--;
		return stack[SP];
	}
	uint16_t machine_state::pop16()
	{
		assert(SP > 1);
		SP -= 2;
		return
			(static_cast<uint32_t>(stack[SP]) << 8) |
			stack[SP + 1];
	}
	uint32_t machine_state::pop32()
	{
		assert(SP > 3);
		SP -= 4;
		return
			(static_cast<uint32_t>(stack[SP]) << 24) |
			(static_cast<uint32_t>(stack[SP + 1]) << 16) |
			(static_cast<uint32_t>(stack[SP + 2]) << 8) |
			stack[SP + 3];
	}
	uint64_t machine_state::pop64()
	{
		assert(SP > 7);
		SP -= 8;
		return
			(static_cast<uint64_t>(stack[SP]) << 56) |
			(static_cast<uint64_t>(stack[SP + 1]) << 48) |
			(static_cast<uint64_t>(stack[SP + 2]) << 40) |
			(static_cast<uint64_t>(stack[SP + 3]) << 32) |
			(static_cast<uint64_t>(stack[SP + 4]) << 24) |
			(static_cast<uint64_t>(stack[SP + 5]) << 16) |
			(static_cast<uint64_t>(stack[SP + 6]) << 8) |
			stack[SP + 7];
	}
#undef SP
#undef IP

	machine_state interpret(program& p)
	{
#define IP state.registers[ip_reg]
#define SP state.registers[sp_reg]
#define FP state.registers[fp_reg]
#define REG state.registers

		machine_state state;

		auto running = true;

		auto chunk = (IP & 0xFFFFFFFF00000000) >> 32;
		auto ip = IP & 0xFFFFFFFF;

		while (p.get_chunk(chunk).has_instruction(ip))
		{
			bytes<10> in = p.get_chunk(chunk).get_instruction<10>(ip);
			op_kind op = byte_to_op(in[0].val);
			uint8_t size = op_size(op);
			switch (op)
			{
				// Arithmetic

			case op_kind::ADD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] + REG[in[3].val]; IP += size; break;
			case op_kind::ADD_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] + in[3].val;      IP += size; break;
			case op_kind::SUB_REG_REG_REG: REG[in[1].val] = REG[in[2].val] - REG[in[3].val]; IP += size; break;
			case op_kind::SUB_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] - in[3].val;      IP += size; break;
			case op_kind::MUL_REG_REG_REG: REG[in[1].val] = REG[in[2].val] * REG[in[3].val]; IP += size; break;
			case op_kind::MOD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] % REG[in[3].val]; IP += size; break;

				// Logic

			case op_kind::GT_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] > REG[in[3].val] ? 1 : 0; IP += size; break;
			case op_kind::GTE_REG_REG_REG: REG[in[1].val] = REG[in[2].val] >= REG[in[3].val] ? 1 : 0; IP += size; break;
			case op_kind::EQ_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] == REG[in[3].val] ? 1 : 0; IP += size; break;
			case op_kind::NEQ_REG_REG_REG: REG[in[1].val] = REG[in[2].val] != REG[in[3].val] ? 1 : 0; IP += size; break;
			case op_kind::AND_REG_REG_REG: REG[in[1].val] = REG[in[2].val] & REG[in[3].val]; IP += size; break;
			case op_kind::OR_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] | REG[in[3].val]; IP += size; break;

				// Moves

			case op_kind::MV8_REG_REG: REG[in[1].val] = REG[in[2].val] & 0x000000FF; IP += size; break;
			case op_kind::MV16_REG_REG:REG[in[1].val] = REG[in[2].val] & 0x0000FFFF; IP += size; break;
			case op_kind::MV32_REG_REG:REG[in[1].val] = REG[in[2].val] & 0xFFFFFFFF; IP += size; break;
			case op_kind::MV64_REG_REG:REG[in[1].val] = REG[in[2].val]; IP += size; break;


			case op_kind::MV8_LOC_REG: state.stack[REG[in[1].val]] =
				REG[in[2].val];
				IP += size; break;
			case op_kind::MV16_LOC_REG:
				state.stack[REG[in[1].val]] = static_cast<int8_t>((REG[in[2].val] & 0xFF00) >> 8);
				state.stack[REG[in[1].val] + 1] = static_cast<int8_t>(REG[in[2].val] & 0xFF);
				IP += size; break;
			case op_kind::MV64_LOC_REG:
				state.stack[REG[in[1].val]] = static_cast<int8_t>((REG[in[2].val] & 0xFF00000000000000) >> 56);
				state.stack[REG[in[1].val] + 1] = static_cast<int8_t>((REG[in[2].val] & 0xFF000000000000) >> 48);
				state.stack[REG[in[1].val] + 2] = static_cast<int8_t>((REG[in[2].val] & 0xFF0000000000) >> 40);
				state.stack[REG[in[1].val] + 3] = static_cast<int8_t>((REG[in[2].val] & 0xFF00000000) >> 32);
				state.stack[REG[in[1].val] + 4] = static_cast<int8_t>((REG[in[2].val] & 0xFF000000) >> 24);
				state.stack[REG[in[1].val] + 5] = static_cast<int8_t>((REG[in[2].val] & 0xFF0000) >> 16);
				state.stack[REG[in[1].val] + 6] = static_cast<int8_t>((REG[in[2].val] & 0xFF00) >> 8);
				state.stack[REG[in[1].val] + 7] = static_cast<int8_t>((REG[in[2].val] & 0xFF));
				IP += size; break;

			case op_kind::MV8_REG_LOC: REG[in[1].val] =
				state.stack[REG[in[2].val]];
				IP += size; break;
			case op_kind::MV16_REG_LOC: REG[in[1].val] =
				(static_cast<uint16_t>(state.stack[REG[in[2].val]]) << 8) |
				state.stack[REG[in[2].val] + 1];
				IP += size; break;
			case op_kind::MV32_REG_LOC: REG[in[1].val] =
				(static_cast<uint32_t>(state.stack[REG[in[2].val]]) << 24) |
				(static_cast<uint32_t>(state.stack[REG[in[2].val] + 1]) << 16) |
				(static_cast<uint32_t>(state.stack[REG[in[2].val] + 2]) << 8) |
				state.stack[REG[in[2].val] + 3];
				IP += size; break;
			case op_kind::MV64_REG_LOC: REG[in[1].val] =
				(static_cast<uint64_t>(state.stack[REG[in[2].val]]) << 56) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 1]) << 48) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 2]) << 40) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 3]) << 32) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 4]) << 24) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 5]) << 16) |
				(static_cast<uint64_t>(state.stack[REG[in[2].val] + 6]) << 8) |
				state.stack[REG[in[2].val] + 7];
				IP += size; break;

			case op_kind::MV_REG_SP: REG[in[1].val] = SP; IP += size; break;
			case op_kind::MV_REG_IP: REG[in[1].val] = IP; IP += size; break;

			case op_kind::MV_REG_UI8:  REG[in[1].val] = read_ui8(bytes<1>{in[2]});
				IP += size;
				break;
			case op_kind::MV_REG_UI16: REG[in[1].val] = read_ui16(bytes<2>{in[2], in[3]});
				IP += size;
				break;
			case op_kind::MV_REG_UI32: REG[in[1].val] = read_ui32(bytes<4>{in[2], in[3], in[4], in[5]});
				IP += size;
				break;
			case op_kind::MV_REG_UI64: REG[in[1].val] = read_ui64(bytes<8>{in[2], in[3], in[4], in[5], in[6], in[7], in[8], in[9]});			IP += 10; break;
				IP += size;
				break;

			case op_kind::MV_REG_I8:  REG[in[1].val] = read_i8(bytes<1>{in[2]});
				IP += size;
				break;
			case op_kind::MV_REG_I16: REG[in[1].val] = read_i16(bytes<2>{in[2], in[3]});
				IP += size;
				break;
			case op_kind::MV_REG_I64:
				REG[in[1].val] = read_i64(bytes<8>{in[2], in[3], in[4], in[5], in[6], in[7], in[8], in[9]});
				IP += size;
				break;

				// Push

			case op_kind::PUSH8_REG:  state.push8(static_cast<uint8_t>(REG[in[1].val]));   IP += size; break;
			case op_kind::PUSH16_REG: state.push16(static_cast<uint16_t>(REG[in[1].val])); IP += size; break;
			case op_kind::PUSH32_REG: state.push32(static_cast<uint32_t>(REG[in[1].val])); IP += size; break;
			case op_kind::PUSH64_REG: state.push64(static_cast<uint64_t>(REG[in[1].val])); IP += size; break;

				// Pop

			case op_kind::POP8_REG:  REG[in[1].val] = state.pop8();  IP += size; break;
			case op_kind::POP16_REG: REG[in[1].val] = state.pop16(); IP += size; break;
			case op_kind::POP32_REG: REG[in[1].val] = state.pop32(); IP += size; break;
			case op_kind::POP64_REG: REG[in[1].val] = state.pop64(); IP += size; break;

				// Jumps

			case op_kind::JMPR_I32:     IP += read_i32(bytes<4>{in[1].val, in[2].val, in[3].val, in[4].val }); break;
			case op_kind::JRNZ_REG_I32: IP += REG[in[1].val] != 0 ? read_i32(bytes<4>{in[2].val, in[3].val, in[4].val, in[5].val }) : op_size(op_kind::JRNZ_REG_I32); break;
			case op_kind::JRZ_REG_I32:  IP += REG[in[1].val] == 0 ? read_i32(bytes<4>{in[2].val, in[3].val, in[4].val, in[5].val }) : op_size(op_kind::JRZ_REG_I32); break;

				// Call

			case op_kind::CALL_UI64:
				state.push64(FP);
				state.push64(IP + op_size(op_kind::CALL_UI64));
				FP = SP;
				IP = read_ui64(bytes<8>{in[1], in[2], in[3], in[4], in[5], in[6], in[7], in[8]});
				break;

				// Return

			case op_kind::RET_UI8: {
				auto in_size = in[1].val;
				assert(in_size >= 0);
				auto return_size = SP - FP;
				assert(return_size >= 0);

				SP = FP;
				IP = state.pop64();
				FP = state.pop64();
				SP -= in_size;
				break;
			}

				// Nops

			case op_kind::NOP: case op_kind::LBL_UI32: IP++; break;

				// Error

			default: throw std::runtime_error("Unknown instruction: chunk " + std::to_string(chunk) + ", ip " + std::to_string(ip));
			}

			chunk = (IP & 0xFFFFFFFF00000000) >> 32;
			ip = IP & 0xFFFFFFFF;
		}

		return state;
#undef FP
#undef SP
#undef IP
#undef REG
	}
}

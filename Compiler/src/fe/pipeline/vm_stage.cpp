#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <string>

namespace fe::vm
{
#define IP registers[ip_reg]
#define SP registers[sp_reg]
#define FP registers[fp_reg]
	void machine_state::push8(uint8_t b)
	{
		assert(SP < stack_size);
		stack[SP] = b;
		SP++;
	}
	void machine_state::push16(uint16_t b)
	{
		assert(SP < stack_size - 1);
		*reinterpret_cast<uint16_t*>(&stack[SP]) = b;
		SP += 2;
	}
	void machine_state::push32(uint32_t b)
	{
		assert(SP < stack_size - 3);
		*reinterpret_cast<uint32_t*>(&stack[SP]) = b;
		SP += 4;
	}
	void machine_state::push64(uint64_t b)
	{
		assert(SP < stack_size - 7);
		*reinterpret_cast<uint64_t*>(&stack[SP]) = b;
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
		return *reinterpret_cast<uint16_t*>(&stack[SP]);
	}
	uint32_t machine_state::pop32()
	{
		assert(SP > 3);
		SP -= 4;
		return *reinterpret_cast<uint32_t*>(&stack[SP]);
	}
	uint64_t machine_state::pop64()
	{
		assert(SP > 7);
		SP -= 8;
		return *reinterpret_cast<uint64_t*>(&stack[SP]);
	}

	void machine_state::ret(uint8_t in_size)
	{
		SP = FP;
		IP = pop64();
		FP = pop64();
		SP -= in_size;
	}
#undef FP
#undef SP
#undef IP

	machine_state interpret(executable& e)
	{
#define IP state.registers[ip_reg]
#define SP state.registers[sp_reg]
#define FP state.registers[fp_reg]
#define REG state.registers

		machine_state state;

		auto running = true;

		auto chunk = (IP & 0xFFFFFFFF00000000) >> 32;
		auto ip = IP & 0xFFFFFFFF;

		int count = 0;

		while (e.functions[chunk].is_native() || (e.functions[chunk].is_bytecode() && e.functions[chunk].get_bytecode().has_instruction(ip)))
		{
			if (e.functions[chunk].is_native())
			{
				e.functions[chunk].get_native_code()(state);
			}
			else
			{
				auto& bc = e.functions[chunk].get_bytecode();
				const byte* in = bc.get_instruction(ip);
				op_kind op = byte_to_op(in[0].val);

				switch (op)
				{

					// Arithmetic

				case op_kind::ADD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] + REG[in[3].val]; IP += ct_op_size<op_kind::ADD_REG_REG_REG>::value; break;
				case op_kind::ADD_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] + static_cast<int64_t>(in[3].val); IP += ct_op_size<op_kind::ADD_REG_REG_UI8>::value; break;
				case op_kind::SUB_REG_REG_REG: REG[in[1].val] = REG[in[2].val] - REG[in[3].val]; IP += ct_op_size<op_kind::SUB_REG_REG_REG>::value; break;
				case op_kind::SUB_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] - static_cast<int64_t>(in[3].val); IP += ct_op_size<op_kind::SUB_REG_REG_UI8>::value; break;
				case op_kind::MUL_REG_REG_REG: REG[in[1].val] = REG[in[2].val] * REG[in[3].val]; IP += ct_op_size<op_kind::MUL_REG_REG_REG>::value; break;
				case op_kind::MOD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] % REG[in[3].val]; IP += ct_op_size<op_kind::MOD_REG_REG_REG>::value; break;

					// Logic

				case op_kind::GT_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] > REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::GTE_REG_REG_REG>::value; break;
				case op_kind::GTE_REG_REG_REG: REG[in[1].val] = REG[in[2].val] >= REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::GTE_REG_REG_REG>::value; break;
				case op_kind::LT_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] < REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::LT_REG_REG_REG>::value;  break;
				case op_kind::LTE_REG_REG_REG: REG[in[1].val] = REG[in[2].val] <= REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::LTE_REG_REG_REG>::value; break;
				case op_kind::EQ_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] == REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::NEQ_REG_REG_REG>::value; break;
				case op_kind::NEQ_REG_REG_REG: REG[in[1].val] = REG[in[2].val] != REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::NEQ_REG_REG_REG>::value; break;
				case op_kind::AND_REG_REG_REG: REG[in[1].val] = REG[in[2].val] & REG[in[3].val]; IP += ct_op_size<op_kind::AND_REG_REG_REG>::value; break;
				case op_kind::OR_REG_REG_REG:  REG[in[1].val] = REG[in[2].val] | REG[in[3].val]; IP += ct_op_size<op_kind::OR_REG_REG_REG>::value; break;

					// Moves

				case op_kind::MV8_REG_REG: REG[in[1].val] = REG[in[2].val] & 0x000000FF; IP += ct_op_size<op_kind::MV8_REG_REG>::value; break;
				case op_kind::MV16_REG_REG:REG[in[1].val] = REG[in[2].val] & 0x0000FFFF; IP += ct_op_size<op_kind::MV16_REG_REG>::value; break;
				case op_kind::MV32_REG_REG:REG[in[1].val] = REG[in[2].val] & 0xFFFFFFFF; IP += ct_op_size<op_kind::MV32_REG_REG>::value; break;
				case op_kind::MV64_REG_REG:REG[in[1].val] = REG[in[2].val]; IP += ct_op_size<op_kind::MV64_REG_REG>::value; break;


				case op_kind::MV8_LOC_REG:  state.stack[REG[in[1].val]] = REG[in[2].val]; IP += ct_op_size<op_kind::MV8_LOC_REG>::value;  break;
				case op_kind::MV16_LOC_REG: *reinterpret_cast<uint16_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV16_LOC_REG>::value; break;
				case op_kind::MV32_LOC_REG: *reinterpret_cast<uint32_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV32_LOC_REG>::value; break;
				case op_kind::MV64_LOC_REG: *reinterpret_cast<uint64_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV64_LOC_REG>::value; break;

				case op_kind::MV8_REG_LOC:  REG[in[1].val] = state.stack[REG[in[2].val]]; IP += ct_op_size<op_kind::MV8_REG_LOC>::value; break;
				case op_kind::MV16_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint16_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV16_REG_LOC>::value; break;
				case op_kind::MV32_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint32_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV32_REG_LOC>::value; break;
				case op_kind::MV64_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint64_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV64_REG_LOC>::value; break;

				case op_kind::MV_REG_SP: REG[in[1].val] = SP; IP += ct_op_size<op_kind::MV_REG_SP>::value; break;
				case op_kind::MV_REG_IP: REG[in[1].val] = IP; IP += ct_op_size<op_kind::MV_REG_IP>::value; break;

				case op_kind::MV_REG_UI8:  REG[in[1].val] = read_ui8(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI8>::value; break;
				case op_kind::MV_REG_UI16: REG[in[1].val] = read_ui16(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI16>::value; break;
				case op_kind::MV_REG_UI32: REG[in[1].val] = read_ui32(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI32>::value; break;
				case op_kind::MV_REG_UI64: REG[in[1].val] = read_ui64(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI64>::value; break;

				case op_kind::MV_REG_I8:  REG[in[1].val] = read_i8(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I8>::value; break;
				case op_kind::MV_REG_I16: REG[in[1].val] = read_i16(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I16>::value; break;
				case op_kind::MV_REG_I64: REG[in[1].val] = read_i64(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I64>::value; break;

					// Push

				case op_kind::PUSH8_REG:  state.push8(static_cast<uint8_t>(REG[in[1].val]));   IP += ct_op_size<op_kind::PUSH8_REG>::value; break;
				case op_kind::PUSH16_REG: state.push16(static_cast<uint16_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH16_REG>::value; break;
				case op_kind::PUSH32_REG: state.push32(static_cast<uint32_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH32_REG>::value; break;
				case op_kind::PUSH64_REG: state.push64(static_cast<uint64_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH64_REG>::value; break;

					// Pop

				case op_kind::POP8_REG:  REG[in[1].val] = state.pop8();  IP += ct_op_size<op_kind::POP8_REG>::value; break;
				case op_kind::POP16_REG: REG[in[1].val] = state.pop16(); IP += ct_op_size<op_kind::POP16_REG>::value; break;
				case op_kind::POP32_REG: REG[in[1].val] = state.pop32(); IP += ct_op_size<op_kind::POP32_REG>::value; break;
				case op_kind::POP64_REG: REG[in[1].val] = state.pop64(); IP += ct_op_size<op_kind::POP64_REG>::value; break;

					// Jumps

				case op_kind::JMPR_I32:     IP += read_i32(&in[1].val); break;
				case op_kind::JRNZ_REG_I32: IP += REG[in[1].val] != 0 ? read_i32(&in[2].val) : ct_op_size<op_kind::JRNZ_REG_I32>::value; break;
				case op_kind::JRZ_REG_I32:  IP += REG[in[1].val] == 0 ? read_i32(&in[2].val) : ct_op_size<op_kind::JRZ_REG_I32>::value; break;

					// Call

				case op_kind::CALL_UI64:
				{
					state.push64(FP);
					state.push64(IP + ct_op_size<op_kind::CALL_UI64>::value);
					FP = SP;
					IP = read_ui64(&in[1].val);
					break;
				}

				// Return

				case op_kind::RET_UI8: {
					state.ret(in[1].val);
					count++;
					break;
				}

									   // Nops

				case op_kind::NOP: case op_kind::LBL_UI32: IP++; break;

					// Error

				default: throw std::runtime_error("Unknown instruction: chunk " + std::to_string(chunk) + ", ip " + std::to_string(ip));
				}
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

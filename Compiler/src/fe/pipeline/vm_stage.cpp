#include "fe/pipeline/vm_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <chrono>

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
		//assert(SP < stack_size - 7);
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
		//assert(SP > 7);
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

	extern "C" uint16_t* vm_init();
	extern "C" uint64_t* vm_interpret(const byte* first);

	direct_threaded_executable preprocess(executable& e, uint16_t* handlers)
	{
		auto ops_between = [](executable& e, byte* a, byte* b) -> int32_t {
			byte* min = a > b ? b : a;
			byte* max = a > b ? a : b;

			int32_t count = 0;
			for (byte* i = min; i < max;)
			{
				count++;
				i += op_size(byte_to_op(i->val));
			}

			return count;
		};

		bytecode bc;
		auto& byte_data = bc.data();
			
		for (byte* i : e)
		{
			auto op = byte_to_op(i->val);
			auto size = op_size(op);

			uint16_t offset = handlers[i->val];
			byte* offset_as_bytes = reinterpret_cast<byte*>(&offset);
			byte_data.push_back(offset_as_bytes[0]);
			byte_data.push_back(offset_as_bytes[1]);

			switch (op)
			{
			case op_kind::CALL_UI64: {
				auto offset = read_i64(*reinterpret_cast<bytes<8>*>(i + 1));
				auto add_offset = ops_between(e, i, i + offset);
				*reinterpret_cast<bytes<8>*>(i + 1) = make_i64(offset + (offset > 0 ? add_offset : -add_offset));
				break;
			}
			case op_kind::JMPR_I32: {
				auto offset = read_i32(*reinterpret_cast<bytes<4>*>(i + 1));
				auto add_offset = ops_between(e, i, i + offset);
				*reinterpret_cast<bytes<4>*>(i + 1) = make_i32(offset + (offset > 0 ? add_offset : -add_offset));
				break;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32: {
				auto offset = read_i32(*reinterpret_cast<bytes<4>*>(i + 2));
				auto add_offset = ops_between(e, i, i + offset);
				*reinterpret_cast<bytes<4>*>(i + 2) = make_i32(offset + (offset > 0 ? add_offset : -add_offset));
				break;
			}
			default:break;
			}

			// Push parameters
			for (int j = 1; j < size; j++)
			{
				byte_data.push_back(j[i].val);
			}
		}

		return direct_threaded_executable(bc, e.native_functions);
	}

	// #todo #cleanup move this into different file together with machine_state and create a common interpretation result 'object'
	machine_state vm_hl_interpret(executable& e, vm_settings& s)
	{
		auto state = machine_state();
		const byte* first_instruction = e.code.get_instruction(near_lbl(0));

#define IP state.registers[ip_reg]
#define SP state.registers[sp_reg]
#define FP state.registers[fp_reg]
#define REG state.registers

		IP = reinterpret_cast<uint64_t>(first_instruction);

		while (1)
		{
			const byte* in = reinterpret_cast<const byte*>(IP);
			switch (byte_to_op(in->val))
			{
				// Arithmetic

			case op_kind::ADD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] + REG[in[3].val]; IP += ct_op_size<op_kind::ADD_REG_REG_REG>::value; continue;
			case op_kind::ADD_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] + static_cast<int64_t>(in[3].val); IP += ct_op_size<op_kind::ADD_REG_REG_UI8>::value; continue;
			case op_kind::SUB_REG_REG_REG: REG[in[1].val] = REG[in[2].val] - REG[in[3].val]; IP += ct_op_size<op_kind::SUB_REG_REG_REG>::value; continue;
			case op_kind::SUB_REG_REG_UI8: REG[in[1].val] = REG[in[2].val] - static_cast<int64_t>(in[3].val); IP += ct_op_size<op_kind::SUB_REG_REG_UI8>::value; continue;
			case op_kind::MUL_REG_REG_REG: REG[in[1].val] = REG[in[2].val] * REG[in[3].val]; IP += ct_op_size<op_kind::MUL_REG_REG_REG>::value; continue;
			case op_kind::MOD_REG_REG_REG: REG[in[1].val] = REG[in[2].val] % REG[in[3].val]; IP += ct_op_size<op_kind::MOD_REG_REG_REG>::value; continue;

				// Logic

			case op_kind::GT_REG_REG_REG: REG[in[1].val] = REG[in[2].val] > REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::GTE_REG_REG_REG>::value; continue;
			case op_kind::GTE_REG_REG_REG: REG[in[1].val] = REG[in[2].val] >= REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::GTE_REG_REG_REG>::value; continue;
			case op_kind::LT_REG_REG_REG: REG[in[1].val] = REG[in[2].val] < REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::LT_REG_REG_REG>::value;  continue;
			case op_kind::LTE_REG_REG_REG: REG[in[1].val] = REG[in[2].val] <= REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::LTE_REG_REG_REG>::value; continue;
			case op_kind::EQ_REG_REG_REG: REG[in[1].val] = REG[in[2].val] == REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::NEQ_REG_REG_REG>::value; continue;
			case op_kind::NEQ_REG_REG_REG: REG[in[1].val] = REG[in[2].val] != REG[in[3].val] ? 1 : 0; IP += ct_op_size<op_kind::NEQ_REG_REG_REG>::value; continue;
			case op_kind::AND_REG_REG_REG: REG[in[1].val] = REG[in[2].val] & REG[in[3].val]; IP += ct_op_size<op_kind::AND_REG_REG_REG>::value; continue;
			case op_kind::OR_REG_REG_REG: REG[in[1].val] = REG[in[2].val] | REG[in[3].val]; IP += ct_op_size<op_kind::OR_REG_REG_REG>::value; continue;

				// Moves

			case op_kind::MV8_REG_REG: REG[in[1].val] = REG[in[2].val] & 0x000000FF; IP += ct_op_size<op_kind::MV8_REG_REG>::value; continue;
			case op_kind::MV16_REG_REG: REG[in[1].val] = REG[in[2].val] & 0x0000FFFF; IP += ct_op_size<op_kind::MV16_REG_REG>::value; continue;
			case op_kind::MV32_REG_REG: REG[in[1].val] = REG[in[2].val] & 0xFFFFFFFF; IP += ct_op_size<op_kind::MV32_REG_REG>::value; continue;
			case op_kind::MV64_REG_REG: REG[in[1].val] = REG[in[2].val]; IP += ct_op_size<op_kind::MV64_REG_REG>::value; continue;


			case op_kind::MV8_LOC_REG: state.stack[REG[in[1].val]] = REG[in[2].val]; IP += ct_op_size<op_kind::MV8_LOC_REG>::value;  continue;
			case op_kind::MV16_LOC_REG: *reinterpret_cast<uint16_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV16_LOC_REG>::value; continue;
			case op_kind::MV32_LOC_REG: *reinterpret_cast<uint32_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV32_LOC_REG>::value; continue;
			case op_kind::MV64_LOC_REG: *reinterpret_cast<uint64_t*>(&state.stack[REG[in[1].val]]) = REG[in[2].val]; IP += ct_op_size<op_kind::MV64_LOC_REG>::value; continue;

			case op_kind::MV8_REG_LOC: REG[in[1].val] = state.stack[REG[in[2].val]]; IP += ct_op_size<op_kind::MV8_REG_LOC>::value; continue;
			case op_kind::MV16_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint16_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV16_REG_LOC>::value; continue;
			case op_kind::MV32_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint32_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV32_REG_LOC>::value; continue;
			case op_kind::MV64_REG_LOC: REG[in[1].val] = *reinterpret_cast<uint64_t*>(&state.stack[REG[in[2].val]]); IP += ct_op_size<op_kind::MV64_REG_LOC>::value; continue;

			case op_kind::MV_REG_SP: REG[in[1].val] = SP; IP += ct_op_size<op_kind::MV_REG_SP>::value; continue;
			case op_kind::MV_REG_IP: REG[in[1].val] = IP; IP += ct_op_size<op_kind::MV_REG_IP>::value; continue;

			case op_kind::MV_REG_UI8: REG[in[1].val] = read_ui8(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI8>::value; continue;
			case op_kind::MV_REG_UI16: REG[in[1].val] = read_ui16(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI16>::value; continue;
			case op_kind::MV_REG_UI32: REG[in[1].val] = read_ui32(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI32>::value; continue;
			case op_kind::MV_REG_UI64: REG[in[1].val] = read_ui64(&in[2].val); IP += ct_op_size<op_kind::MV_REG_UI64>::value; continue;

			case op_kind::MV_REG_I8: REG[in[1].val] = read_i8(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I8>::value; continue;
			case op_kind::MV_REG_I16: REG[in[1].val] = read_i16(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I16>::value; continue;
			case op_kind::MV_REG_I64: REG[in[1].val] = read_i64(&in[2].val); IP += ct_op_size<op_kind::MV_REG_I64>::value; continue;

				// Push

			case op_kind::PUSH8_REG: state.push8(static_cast<uint8_t>(REG[in[1].val]));   IP += ct_op_size<op_kind::PUSH8_REG>::value; continue;
			case op_kind::PUSH16_REG: state.push16(static_cast<uint16_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH16_REG>::value; continue;
			case op_kind::PUSH32_REG: state.push32(static_cast<uint32_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH32_REG>::value; continue;
			case op_kind::PUSH64_REG: state.push64(static_cast<uint64_t>(REG[in[1].val])); IP += ct_op_size<op_kind::PUSH64_REG>::value; continue;

				// Pop

			case op_kind::POP8_REG: REG[in[1].val] = state.pop8();  IP += ct_op_size<op_kind::POP8_REG>::value; continue;
			case op_kind::POP16_REG: REG[in[1].val] = state.pop16(); IP += ct_op_size<op_kind::POP16_REG>::value; continue;
			case op_kind::POP32_REG: REG[in[1].val] = state.pop32(); IP += ct_op_size<op_kind::POP32_REG>::value; continue;
			case op_kind::POP64_REG: REG[in[1].val] = state.pop64(); IP += ct_op_size<op_kind::POP64_REG>::value; continue;

				// Jumps

			case op_kind::JMPR_I32: IP += read_i32(&in[1].val); continue;
			case op_kind::JRNZ_REG_I32: IP += REG[in[1].val] != 0 ? read_i32(&in[2].val) : ct_op_size<op_kind::JRNZ_REG_I32>::value; continue;
			case op_kind::JRZ_REG_I32: IP += REG[in[1].val] == 0 ? read_i32(&in[2].val) : ct_op_size<op_kind::JRZ_REG_I32>::value; continue;

			case op_kind::SALLOC_REG_UI8: REG[in[1].val] = SP; SP += read_ui8(&in[2].val); IP += ct_op_size<op_kind::SALLOC_REG_UI8>::value; continue;
			case op_kind::SDEALLOC_UI8: SP -= read_ui8(&in[1].val); IP += ct_op_size<op_kind::SDEALLOC_UI8>::value; continue;

				// Call

			case op_kind::CALL_UI64:
			{
				state.push64(FP);
				state.push64(IP + ct_op_size<op_kind::CALL_UI64>::value);
				FP = SP;
				uint64_t offset = read_ui64(&in[1].val);
				IP += offset;
				continue;
			}

			case op_kind::CALL_NATIVE_UI64:
			{
				auto id = read_ui64(&in[1].val);
				e.native_functions[id](state);
				IP += ct_op_size<op_kind::CALL_NATIVE_UI64>::value;
				continue;
			}

			// Return

			case op_kind::RET_UI8:
			{
				state.ret(in[1].val);
				continue;
			}

			case op_kind::EXIT:
				return state;

				// Nops

			case op_kind::NOP:
			case op_kind::LBL_UI32: IP++; continue;

				// Error

			default: throw std::runtime_error("Unknown instruction");
			}
		}

#undef FP
#undef SP
#undef IP
#undef REG
		throw std::runtime_error("Program did not exit with op kind EXIT");
	}

	machine_state interpret(executable& e, vm_settings& s)
	{
		const byte* first_instruction = e.code.get_instruction(near_lbl(0));

		if (s.print_code)
		{
			for (byte* op : e)
			{
				auto size = op_size(byte_to_op(op->val));
				std::cout << op_to_string(byte_to_op(op->val));
				for (int j = 1; j < size; j++)
				{
					std::cout << " " << std::to_string((op + j)->val);
				}
				std::cout << "\n";
			}
		}

		auto before = std::chrono::high_resolution_clock::now();
		machine_state res;

		switch (s.implementation)
		{
		case vm_implementation::asm_: {
			uint16_t* handlers = vm_init();
			auto direct_threaded = preprocess(e, handlers);

			// Skip timing the preprocessing step (just to measure actual execution speed)
			before = std::chrono::high_resolution_clock::now();

			uint64_t* res_registers = vm_interpret(direct_threaded.code.get_instruction(0));

			// Fill the res machine_state so we can check/test results
			for (int i = 0; i < 64; i++)
				res.registers[i] = res_registers[i];

			break;
		}
		case vm_implementation::cpp: {
			res = vm_hl_interpret(e, s);
			break;
		}
		default:
			throw std::runtime_error("Unknown VM implementation strategy");
		}

		auto after = std::chrono::high_resolution_clock::now();
		long long time = std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();

		std::cout << "time: " << time << std::endl;
		return res;
	}
}

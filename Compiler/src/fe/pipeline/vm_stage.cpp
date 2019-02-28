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

	extern "C" uint16_t* vm_init(native_function_ptr*);
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

		// Replace each op with the offset of its handler to the first handler (i.e. direct threaded)
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
			case op_kind::CALL_UI64_UI8_UI8_UI8: {
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

	machine_state interpret(executable& e, vm_settings& s)
	{
		const byte* first_instruction = e.code.get_instruction(near_lbl(0));

		auto before = std::chrono::high_resolution_clock::now();
		machine_state res;

		switch (s.implementation)
		{
		case vm_implementation::asm_: {
			uint16_t* op_handlers = vm_init(e.native_functions.data());
			auto direct_threaded = preprocess(e, op_handlers);

			// Skip timing the preprocessing step (just to measure actual execution speed) 
			// #todo remove this at some point 
			before = std::chrono::high_resolution_clock::now();

			uint64_t* res_registers = vm_interpret(direct_threaded.code.get_instruction(0));

			// Fill the res machine_state so we can check/test results
			for (int i = 0; i < 64; i++)
				res.registers[i] = res_registers[i];

			break;
		}
		default:
			throw std::runtime_error("Unknown VM implementation strategy");
		}

		if (s.print_time)
		{
			auto after = std::chrono::high_resolution_clock::now();
			long long time = std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();

			std::cout << "time: " << time << std::endl;
		}

		return res;
	}
}

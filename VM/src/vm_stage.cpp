#include "vm_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <chrono>

namespace fe::vm
{
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

	void interpret(executable& e, vm_settings& s)
	{
		const byte* first_instruction = e.code.get_instruction(near_lbl(0));

		uint16_t* op_handlers = vm_init(e.native_functions.data());
		auto direct_threaded = preprocess(e, op_handlers);

		vm_interpret(direct_threaded.code.get_instruction(0));
	}
}

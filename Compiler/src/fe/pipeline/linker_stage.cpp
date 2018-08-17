#include "fe/pipeline/linker_stage.h"

namespace fe::vm
{
	executable link(program p)
	{
		std::unordered_map<uint32_t, far_lbl> label_locations;

		// First gather all label locations
		for (int i = 0; i < p.function_count(); i++)
		{
			auto& function = p.get_function(i);
			auto& bytes = function.second.data();
			for (int j = 0; j < bytes.size();)
			{
				byte op = bytes[j];
				if (byte_to_op(op.val) == op_kind::LBL_UI32)
				{
					uint32_t id = read_ui32({ bytes[j + 1], bytes[j + 2], bytes[j + 3], bytes[j + 4] });
					// Replace label with nops as labels don't do anything
					for (int k = 0; k < op_size(op_kind::LBL_UI32); k++) bytes[j + k] = op_to_byte(op_kind::NOP);
					label_locations.insert({ id, far_lbl(i, j) });
				}

				j += op_size(byte_to_op(op.val));
			}
		}

		// Then replace jumps to label with jumps to location
		for (int i = 0; i < p.function_count(); i++)
		{
			auto& function = p.get_function(i);
			auto& data = function.second.data();
			for (int j = 0; j < data.size();)
			{
				byte op = data[j];
				switch (byte_to_op(op.val))
				{
				case op_kind::JMPR_I32:
				{
					auto label = read_ui32(bytes<4>{data[j + 1], data[j + 2], data[j + 3], data[j + 4]});
					auto offset = make_i32(label_locations.at(label).ip - j);
					// #todo abstract this into function 
					data[j + 1] = offset[0];
					data[j + 2] = offset[1];
					data[j + 3] = offset[2];
					data[j + 4] = offset[3];
				}
				case op_kind::JRNZ_REG_I32:
				case op_kind::JRZ_REG_I32:
				{
					auto label = read_ui32(bytes<4>{data[j + 2], data[j + 3], data[j + 4], data[j + 5]});
					auto offset = make_i32(label_locations.at(label).ip - j);
					data[j + 2] = offset[0];
					data[j + 3] = offset[1];
					data[j + 4] = offset[2];
					data[j + 5] = offset[3];
					break;
				}
				case op_kind::CALL_UI64:
				{
					auto label = read_ui32(bytes<4>{data[j + 5], data[j + 6], data[j + 7], data[j + 8]});
					auto offset = make_ui64(label_locations.at(label).make_ip());
					data[j + 1] = offset[0];
					data[j + 2] = offset[1];
					data[j + 3] = offset[2];
					data[j + 4] = offset[3];
					data[j + 5] = offset[4];
					data[j + 6] = offset[5];
					data[j + 7] = offset[6];
					data[j + 8] = offset[7];
					break;
				}
				}

				j += op_size(byte_to_op(op.val));
			}
		}

		auto module = p.get_code();
		std::vector<bytecode> code_segments;
		std::transform(module.begin(), module.end(), std::back_inserter(code_segments), [](auto& func) { return func.second; });

		return executable(code_segments, p.get_interrupts());
	}
}
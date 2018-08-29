#include "fe/pipeline/linker_stage.h"
#include <unordered_map>

namespace fe::vm
{
	executable link(program p)
	{
		std::vector<function> chunks;
		std::unordered_map<name, uint32_t> function_locations;

		// Gather all function locations
		std::vector<function>& funcs = p.get_code();
		for (auto& func : funcs)
		{
			chunks.push_back(func);
			function_locations.insert({ func.get_name(), chunks.size() - 1 });
		}

		// Gather all label locations
		for (int i = 0; i < chunks.size(); i++)
		{
			auto& chunk = chunks.at(i);
			if (chunk.is_native()) continue;

			auto& data = chunk.get_bytecode().data();

			std::unordered_map<uint32_t, far_lbl> label_locations;
			for (int j = 0; j < data.size();/*j is incremented later*/)
			{
				byte op = data[j];
				if (byte_to_op(op.val) == op_kind::LBL_UI32)
				{
					uint32_t id = read_ui32({ data[j + 1], data[j + 2], data[j + 3], data[j + 4] });
					// Replace label with nops as labels don't do anything
					for (int k = 0; k < op_size(op_kind::LBL_UI32); k++) data[j + k] = op_to_byte(op_kind::NOP);
					label_locations.insert({ id, far_lbl(i, j) });
				}

				j += op_size(byte_to_op(op.val));
			}

			for (int j = 0; j < data.size();)
			{
				byte op = data[j];
				switch (byte_to_op(op.val))
				{
					// Jumps are all relative to a label within the same bytecode

				case op_kind::JMPR_I32:
				{
					auto label = read_ui32(bytes<4>{data[j + 1], data[j + 2], data[j + 3], data[j + 4]});
					*(reinterpret_cast<int32_t*>(&data[j + 1])) = static_cast<int32_t>(label_locations.at(label).ip - j);
					break;
				}
				case op_kind::JRNZ_REG_I32:
				case op_kind::JRZ_REG_I32:
				{
					auto label = read_ui32(bytes<4>{data[j + 2], data[j + 3], data[j + 4], data[j + 5]});
					*(reinterpret_cast<int32_t*>(&data[j + 2])) = static_cast<int32_t>(label_locations.at(label).ip - j);
					break;
				}

				// Calls reference other bytecode

				case op_kind::CALL_UI64:
				{
					auto label = read_ui64(&data[j + 1].val);
					auto function_name = chunks[i].get_symbols().at(label);
					auto function_location = function_locations.at(function_name);

					*(reinterpret_cast<uint64_t*>(&data[j + 1])) = far_lbl(function_location, 0).make_ip();
					break;
				}
				}

				j += op_size(byte_to_op(op.val));
			}
		}

		return executable(chunks);
	}
}
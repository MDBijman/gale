#include "fe/pipeline/linker_stage.h"
#include <assert.h>
#include <unordered_map>

namespace fe::vm
{
	executable link(program p)
	{
		bytecode code;

		std::unordered_map<name, int64_t> function_locations;

		std::unordered_map<name, uint64_t> native_function_locations;

		// chunk_locations[chunk_id] = locations of chunk
		std::vector<int64_t> chunk_locations;

		// Gather all function locations
		std::vector<function> &funcs = p.get_code();

		uint32_t chunk_id = 0;
		for (auto &func : funcs)
		{
			if (func.is_bytecode())
			{
				chunk_locations.push_back(code.size());
				chunk_id++;

				function_locations.insert({ func.get_name(), code.data().size() });
				code.append(func.get_bytecode());
			}
			else if (func.is_native())
			{
				native_function_locations.insert(
				  { func.get_name(), func.get_native_function_id() });
			}
		}

		std::unordered_map<uint32_t, int64_t> label_locations;
		auto &data = code.data();

		// Gather all label locations
		for (int i = 0; i < code.size();)
		{
			byte op = data[i];

			if (byte_to_op(op.val) == op_kind::LBL_UI32)
			{
				uint32_t id =
				  read_ui32({ data[i + 1], data[i + 2], data[i + 3], data[i + 4] });
				// Replace label with nops as labels don't do anything
				for (int k = 0; k < op_size(op_kind::LBL_UI32); k++)
					data[i + k] = op_to_byte(op_kind::NOP);
				// Labels must be unique
				assert(label_locations.find(id) == label_locations.end());
				label_locations.insert({ id, i });
			}

			i += op_size(byte_to_op(op.val));
		}

		// The minimal chunk id is the number of chunks that have at least passed during
		// iteration It is used to optimize finding the chunk of a position i (used for
		// resolving calls)
		auto minimal_chunk_id = 0;

		for (int i = 0; i < code.size();)
		{
			byte op = data[i];
			switch (byte_to_op(op.val))
			{
				// Jumps are all relative to a label within the same bytecode

			case op_kind::JMPR_I32:
			{
				auto label = read_ui32(
				  bytes<4>{ data[i + 1], data[i + 2], data[i + 3], data[i + 4] });
				auto label_location = label_locations.at(label);
				auto offset = label_location - i;
				*(reinterpret_cast<int32_t *>(&data[i + 1])) = offset;
				break;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32:
			{
				auto label = read_ui32(
				  bytes<4>{ data[i + 2], data[i + 3], data[i + 4], data[i + 5] });
				auto label_location = label_locations.at(label);
				auto offset = label_location - i;
				// assert(offset > std::numeric_limits<int32_t>::min() && offset <
				// std::numeric_limits<int32_t>::max());
				*(reinterpret_cast<int32_t *>(&data[i + 2])) = offset;
				break;
			}

				// Calls reference other bytecode

			case op_kind::CALL_UI64_UI8_UI8_UI8:
			{
				auto sc =
				  std::find_if(chunk_locations.rbegin(), chunk_locations.rend(),
					       [i](auto loc) { return loc <= i; });
				uint32_t current_chunk =
				  std::distance(chunk_locations.begin(), sc.base() - 1);
				minimal_chunk_id = current_chunk;
				auto label = read_ui64(&data[i + 1].val);
				auto function_name = funcs[current_chunk].get_symbols().at(label);

				int64_t function_location;
				if (auto it = function_locations.find(function_name);
				    it != function_locations.end())
				{
					function_location = function_locations.at(function_name);
					*(reinterpret_cast<uint64_t *>(&data[i + 1])) =
					  function_location - i;
				}
				else if (auto it = native_function_locations.find(function_name);
					 it != native_function_locations.end())
				{
					data[i] = op_to_byte(op_kind::CALL_NATIVE_UI64_UI8_UI8);
					*(reinterpret_cast<uint64_t *>(&data[i + 1])) =
					  native_function_locations[function_name];

					data[i + 10] = data[i + 11];
					data[i + 11] = op_to_byte(op_kind::NOP);
				}
				else
				{
					throw std::runtime_error("Cannot find function location: " + function_name);
				}
				

				break;
			}
			}

			i += op_size(byte_to_op(op.val));
		}

		return executable(code);
	}
} // namespace fe::vm
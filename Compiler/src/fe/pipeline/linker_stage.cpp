#include "fe/pipeline/linker_stage.h"
#include <assert.h>
#include <unordered_map>

namespace fe::vm
{
	struct executable_layout
	{
		// Maps a function name to a bytecode index
		std::unordered_map<name, int64_t> function_locations;

		// Maps a function name to a native function id
		std::unordered_map<name, uint64_t> native_function_locations;

		// Maps a function name to a mapping of label ids to bytecode indices
		std::unordered_map<name, std::unordered_map<uint32_t, int64_t>>
		  function_label_locations;

		// Maps a module name to a vector of bytecode indices
		// such that chunk_locations[chunk_id] = bytecode_index
		std::unordered_map<name, std::vector<int64_t>> module_chunk_locations;

		std::vector<int64_t> module_locations;
		size_t total_size = 0;
	};

	/*
	 * Adds the locations of labels in the given function to the layout data.
	 * This is used later to rewrite jumps.
	 */
	void gather_label_locations(const vm::function &fn, executable_layout &layout)
	{
		std::unordered_map<uint32_t, int64_t> locations;

		const auto &data = fn.get_bytecode().data();

		// Gather all label locations
		for (int i = 0; i < data.size();)
		{
			byte op = data[i];

			if (byte_to_op(op.val) == op_kind::LBL_UI32)
			{
				uint32_t id =
				  read_ui32({ data[i + 1], data[i + 2], data[i + 3], data[i + 4] });
				// Labels must be unique
				assert(locations.find(id) == locations.end());
				locations.insert({ id, i });
			}

			i += op_size(byte_to_op(op.val));
		}

		layout.function_label_locations.insert({ fn.get_name(), std::move(locations) });
	}

	/*
	 * Adds the locations of functions, chunks, and labels to the layout data.
	 * This is used later to rewrite jumps and calls.
	 */
	void add_to_layout(const name& mod_name, const vm::module &mod, executable_layout &layout)
	{
		size_t before = layout.total_size;

		layout.module_locations.push_back(layout.total_size);

		layout.module_chunk_locations.insert({ mod_name, {} });

		for (const auto &fn : mod.get_code())
		{
			if (fn.is_bytecode())
			{
				layout.module_chunk_locations[mod_name].push_back(layout.total_size);
				layout.function_locations.insert(
				  { mod_name + "." + fn.get_name(), layout.total_size });
				layout.total_size += fn.get_bytecode().size();

				gather_label_locations(fn, layout);
			}
			else if (fn.is_native())
			{
				layout.native_function_locations.insert(
				  { mod_name + "." + fn.get_name(), fn.get_native_function_id() });
			}
		}

		// This assert doesn't work for purely native modules
		// assert(layout.total_size > before);
	}

	/*
	 * Rewrite the jumps/labels within the given function using the previously generated layout
	 * data. Replaces labels in jump instructions with relative offsets, and labels with NOPs.
	 */
	void rewrite_jumps_and_labels(vm::function &fn, const executable_layout &layout)
	{
		auto &data = fn.get_bytecode().data();
		auto &name = fn.get_name();

		const auto &label_locations = layout.function_label_locations.at(name);

		for (int i = 0; i < data.size();)
		{
			op_kind op = byte_to_op(data[i].val);
			auto size = op_size(op);

			switch (op)
			{

			case op_kind::LBL_UI32:
			{
				// Replace label with nops as labels don't do anything
				for (int k = 0; k < op_size(op_kind::LBL_UI32); k++)
					data[i + k] = op_to_byte(op_kind::NOP);
				break;
			}

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
				// assert(offset > std::numeric_limits<int32_t>::min() &&
				// offset < std::numeric_limits<int32_t>::max());
				*(reinterpret_cast<int32_t *>(&data[i + 2])) = offset;
				break;
			}
			}

			i += size;
		}
	}

	/*
	 * Rewrite the calls within the given function using the previously generated layout
	 * data. Replaces the labels in call instructions with relative offsets.
	 */
	void rewrite_calls(vm::function &fn, const executable_layout &layout)
	{
		// The minimal chunk id is the number of chunks that have at least passed
		// during iteration It is used to optimize finding the chunk of a position i
		// (used for resolving calls)
		auto minimal_chunk_id = 0;

		auto &code = fn.get_bytecode();
		auto &data = code.data();

		for (int i = 0; i < code.size();)
		{
			op_kind op = byte_to_op(data[i].val);
			auto size = op_size(op);

			switch (op)
			{

				// Calls reference other bytecode

			case op_kind::CALL_UI64_UI8_UI8_UI8:
			{
				auto label = read_ui64(&data[i + 1].val);
				auto function_name = fn.get_symbols().at(label);

				int64_t function_location;
				if (auto it = layout.function_locations.find(function_name);
				    it != layout.function_locations.end())
				{
					*(reinterpret_cast<uint64_t *>(&data[i + 1])) =
					  it->second - i;
				}
				else if (auto it = layout.native_function_locations.find(function_name);
					 it != layout.native_function_locations.end())
				{
					data[i] = op_to_byte(op_kind::CALL_NATIVE_UI64_UI8_UI8);
					*(reinterpret_cast<uint64_t *>(&data[i + 1])) = it->second;

					data[i + 10] = data[i + 11];
					data[i + 11] = op_to_byte(op_kind::NOP);
				}
				else
				{
					throw std::runtime_error("Cannot find function location: " +
								 function_name);
				}

				break;
			}
			}

			i += size;
		}
	}

	void add_to_executable(const vm::module &mod, const executable_layout &layout,
			       bytecode &code)
	{
		// Gather all function locations
		const std::vector<function> &fns = mod.get_code();

		for (const auto &fn : fns)
		{
			if (fn.is_bytecode())
			{
				auto function = vm::function(fn);

				rewrite_jumps_and_labels(function, layout);
				rewrite_calls(function, layout);

				code.append(function.get_bytecode());
			}
		}
	}

	executable link(const std::unordered_map<std::string, module> &modules)
	{
		bytecode code;
		executable_layout layout;

		for (const auto name_and_module : modules)
		{
			add_to_layout(name_and_module.first, name_and_module.second, layout);
		}

		for (auto name_and_module : modules)
		{
			add_to_executable(name_and_module.second, layout, code);
		}

		return executable(code);
	}
} // namespace fe::vm
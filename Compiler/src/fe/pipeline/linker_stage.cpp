#include "fe/pipeline/linker_stage.h"
#include <assert.h>
#include <unordered_map>

namespace fe::vm
{
	struct executable_layout
	{
		std::unordered_map<name, int64_t> function_locations;
		std::unordered_map<name, uint64_t> native_function_locations;
		std::unordered_map<name, std::unordered_map<uint32_t, int64_t>>
		  function_label_locations;

		std::vector<int64_t> chunk_locations;
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
	void add_to_layout(const vm::module &mod, executable_layout &layout)
	{
		size_t before = layout.total_size;

		layout.module_locations.push_back(layout.total_size);

		for (const auto &fn : mod.get_code())
		{
			if (fn.is_bytecode())
			{
				layout.chunk_locations.push_back(layout.total_size);
				layout.function_locations.insert(
				  { fn.get_name(), layout.total_size });
				layout.total_size += fn.get_bytecode().size();

				gather_label_locations(fn, layout);
			}
			else if (fn.is_native())
			{
				layout.native_function_locations.insert(
				  { fn.get_name(), fn.get_native_function_id() });
			}
		}

		assert(layout.total_size > before);
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
			byte op = data[i];

			switch (byte_to_op(op.val))
			{

			case op_kind::LBL_UI32:
			{
				// Replace label with nops as labels don't do anything
				for (int k = 0; k < op_size(op_kind::LBL_UI32); k++)
					data[i + k] = op_to_byte(op_kind::NOP);
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

		for (int i = 0; i < code.size();)
		{
			byte op = code.data()[i];
			switch (byte_to_op(op.val))
			{

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
					throw std::runtime_error("Cannot find function location: " +
								 function_name);
				}

				break;
			}
			}

			i += op_size(byte_to_op(op.val));
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
			add_to_layout(name_and_module.second, layout);
		}

		for (auto name_and_module : modules)
		{
			add_to_executable(name_and_module.second, layout, code);
		}

		return executable(code);
	}
} // namespace fe::vm
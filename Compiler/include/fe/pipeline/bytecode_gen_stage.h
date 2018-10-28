#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/bytecode.h"
#include "utils/memory/data_store.h"
#include <unordered_map>
#include <bitset>

namespace fe::vm
{
	using memory_location = uint64_t;
	using variable_location = std::variant<reg, memory_location>;

	class code_gen_state
	{
		// Mapping of nodes to bytecode chunks
		std::unordered_map<node_id, uint8_t> node_to_chunk;

		// Bitfield with 64 entries, one for each register, 1 indicates register is allocated
		std::bitset<64> used_registers;

		std::unordered_map<std::string, core_ast::label> functions;
		core_ast::label next_function_label = { 0 };
		core_ast::label new_function_label() 
		{
			auto copy = next_function_label;
			next_function_label.id++;
			return copy;
		}

		std::unordered_map<uint8_t, uint8_t> var_to_reg;

	public:
		std::vector<reg> clear_temp_registers();
		void set_temp_registers(std::vector<reg>);
		reg alloc_temp_register();
		void dealloc_temp_register(reg r);

		std::vector<reg> clear_saved_registers();
		void set_saved_registers(std::vector<reg>);
		reg alloc_saved_register();
		void dealloc_saved_register(reg r);

		void link_node_chunk(node_id, uint8_t);
		uint8_t chunk_of(node_id);

		reg var_reg(uint8_t var);
		void dealloc_var(uint8_t var);

		core_ast::label function_label(std::string name);
	};

	struct code_gen_result
	{
		code_gen_result();
		code_gen_result(int64_t res_size, far_lbl code_loc, uint32_t code_size);
		int64_t result_size;
		far_lbl code_location;
		uint32_t code_size;
	};

	class program;
	program generate_bytecode(core_ast::ast& n);
}

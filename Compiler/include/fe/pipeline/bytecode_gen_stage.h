#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/bytecode.h"
#include "utils/memory/data_store.h"
#include <unordered_map>

namespace fe::vm
{
	using memory_location = uint64_t;
	using variable_location = std::variant<reg, memory_location>;

	class code_gen_state
	{
		// Mapping of nodes to bytecode chunks
		std::unordered_map<core_ast::node_id, uint8_t> node_to_chunk;

		// Register allocation
		std::vector<reg> in_use;
		reg next_allocation = 0;

		std::unordered_map<std::string, far_lbl> functions;

	public:
		std::vector<reg> clear_registers();
		reg alloc_register();

		void link_node_chunk(core_ast::node_id, uint8_t);
		uint8_t chunk_of(core_ast::node_id);

		void add_function_loc(std::string name, far_lbl l);
		far_lbl function_loc(std::string name);
	};

	struct code_gen_result
	{
		code_gen_result();
		code_gen_result(uint8_t res_size, far_lbl code_loc);
		uint8_t result_size;
		far_lbl code_location;
	};

	class program;
	program generate_bytecode(core_ast::ast& n);
}

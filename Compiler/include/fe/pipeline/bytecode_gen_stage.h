#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/bytecode.h"
#include "utils/memory/data_store.h"

namespace fe::vm
{
	using memory_location = uint64_t;
	using variable_location = std::variant<reg, memory_location>;

	class code_gen_state
	{
		// Mapping of nodes to bytecode chunks
		std::unordered_map<core_ast::node_id, uint8_t> node_to_chunk;

		// Register allocation
		std::unordered_map<std::string, variable_location> identifier_variables;
		std::vector<reg> in_use;
		reg next_allocation = 0;

		std::unordered_map<std::string, far_lbl> functions;
	public:
		std::vector<reg> clear_registers();

		void link_node_chunk(core_ast::node_id, uint8_t);
		uint8_t chunk_of(core_ast::node_id);

		bool is_mapped(const core_ast::identifier&);
		reg alloc_register();
	};

	struct code_gen_info
	{
		code_gen_info();
		code_gen_info(uint8_t res_size, variable_location res_loc, far_lbl code_loc);
		uint8_t result_size;
		variable_location result_location;
		far_lbl code_location;
	};

	class program;
	program generate_bytecode(core_ast::ast& n);
}

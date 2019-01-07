#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/bytecode.h"
#include "utils/memory/data_store.h"
#include "fe/pipeline/core_stack_analysis.h"
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

		// Mapping from function names to code labels
		std::unordered_map<std::string, core_ast::label> functions;

		// Mapping from stack label to the frame size at its location in the ast
		std::unordered_map<uint32_t, uint32_t> stack_label_sizes;

		core_ast::label next_label;

	public:
		code_gen_state(core_ast::label first_label);

		core_ast::function_data current_scope;

		std::vector<reg> clear_registers();
		void set_saved_registers(std::vector<reg>);
		reg alloc_register();
		void dealloc_register(reg r);

		void link_node_chunk(node_id, uint8_t);
		uint8_t chunk_of(node_id);

		core_ast::label get_function_label(const std::string& name);

		std::unordered_map<uint32_t, core_ast::stack_analysis_result> analyzed_functions;

		uint32_t node_pre_stack_size(uint32_t function_id, uint32_t node_id);
		uint32_t node_post_stack_size(uint32_t function_id, uint32_t node_id);
		uint32_t node_diff_stack_size(uint32_t function_id, uint32_t node_id);

		void set_stack_label_size(uint32_t stack_label, uint32_t size);
		uint32_t get_stack_label_size(uint32_t stack_label);
	};

	struct code_gen_result
	{
		code_gen_result();
	};

	class program;
	program generate_bytecode(core_ast::ast& n);
}

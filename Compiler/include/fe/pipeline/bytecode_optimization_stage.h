#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	struct optimization_settings
	{

	};

	/*
	* Program object optimization
	*
	* Passes
	* 1) Builds dependency graph between instructions
	* 2) Removes push/pop pairs that are equivalent to a single register mov 
	* 3)
	*/
	void optimize_program(program& p, optimization_settings& s);

	struct dependency
	{
		uint64_t instruction_id;
		uint64_t depends_on;
	};

	struct function_dependency_graph
	{
		std::vector<dependency> dependencies;

		void add_offset(uint64_t loc, uint32_t size);
	};

	using program_dependency_graph = std::unordered_map<uint64_t, function_dependency_graph>;

	program_dependency_graph build_dependency_graph(program& e);
	bool optimize_dependencies(program& e, program_dependency_graph& g, optimization_settings& s);
	bool optimize_single_ops(program& e, program_dependency_graph& g, optimization_settings& s);

	bool remove_dependantless_instructions(program& e, program_dependency_graph& g);

	/*
	* Executable object optimization
	*
	* Single pass to remove nops that were introduced by removing labels
	*/
	void optimize_executable(executable& p, optimization_settings& s);

	void remove_nops(executable& e, optimization_settings& s);
}
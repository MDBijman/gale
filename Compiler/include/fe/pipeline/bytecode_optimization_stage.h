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
	* 2) 
	* 3)
	*/
	void optimize_program(program& p, optimization_settings& s);

	struct dependency
	{
		uint64_t instruction_id;
		uint64_t depends_on;
	};

	struct dependency_graph
	{
		std::unordered_map<function_id, std::vector<dependency>> dependencies;

		void add_offset(function_id fun, uint64_t loc, uint32_t size);
	};

	dependency_graph build_dependency_graph(program& e);
	bool remove_redundant_dependencies(program& e, dependency_graph& g, optimization_settings& s);

	/*
	* Executable object optimization
	*
	* Single pass to remove nops that were introduced by removing labels
	*/
	void optimize_executable(executable& p, optimization_settings& s);

	void remove_nops(executable& e, optimization_settings& s);
}
#pragma once
#include "fe/data/bytecode.h"
#include <unordered_map>

namespace fe::vm
{
	struct optimization_settings
	{
		bool print_bytecode = false;
	};

	void optimize_module(module& p, optimization_settings& s);

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

	using module_dependency_graph = std::unordered_map<uint64_t, function_dependency_graph>;

	module_dependency_graph build_dependency_graph(module& e);

	bool optimize_single_ops(module& e, module_dependency_graph& g, optimization_settings& s);

	bool remove_dependantless_instructions(module& e, module_dependency_graph& g);

	/*
	* Executable object optimization
	*
	* Single pass to remove nops that were introduced by removing labels
	*/
	void optimize_executable(executable& p, optimization_settings& s);

	void remove_nops(executable& e, optimization_settings& s);
}
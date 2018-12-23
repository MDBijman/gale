#pragma once
#include "fe/data/core_ast.h"
#include <unordered_map>

namespace fe::core_ast
{
	struct stack_analysis_result
	{
		std::unordered_map<node_id, uint32_t> node_stack_sizes;
		std::unordered_map<node_id, uint32_t> pre_node_stack_sizes;
	};

	// Analyze stack size of function node and its children
	stack_analysis_result analyze_stack(node_id n, ast& ast);
	void analyze_stack(node_id n, ast& ast, stack_analysis_result& res);
}
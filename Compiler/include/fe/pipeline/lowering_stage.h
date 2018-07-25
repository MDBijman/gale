#pragma once
#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"

namespace fe::ext_ast
{
	struct lowering_context;
	struct lowering_result;
	core_ast::ast lower(ast& ast);
	lowering_result lower(node& n, ast& ast, core_ast::ast& new_ast, lowering_context&);
}
#pragma once
#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"

namespace fe::ext_ast
{
	core_ast::ast lower(ast& ast);
	core_ast::node_id lower(node& n, ast& ast, core_ast::ast& new_ast);
}
#pragma once
#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"

namespace fe::ext_ast
{
	core_ast::node* lower(node& n, ast& ast);
}
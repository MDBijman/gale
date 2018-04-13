#pragma once
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	types::unique_type typeof(node& n, ast& ast);
	void typecheck(node& n, ast& ast);
}

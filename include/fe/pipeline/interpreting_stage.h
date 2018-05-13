#pragma once
#include "error.h"
#include "fe/data/core_ast.h"

namespace fe::core_ast
{
	values::unique_value interpret(node& n, ast& ast);
	std::variant<values::unique_value, interp_error> interpret(ast& ast);
}
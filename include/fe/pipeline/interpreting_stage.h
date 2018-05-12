#pragma once
#include "error.h"
#include "pipeline.h"


namespace fe::core_ast
{
	std::variant<values::unique_value, interp_error> interpret(ast& ast);
}
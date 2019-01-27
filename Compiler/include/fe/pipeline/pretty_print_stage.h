#pragma once
#include <string>

namespace fe::ext_ast
{
	class ast;

	std::string pretty_print(ast& ast);
}
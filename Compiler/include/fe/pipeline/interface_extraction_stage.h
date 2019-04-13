#pragma once
#include "fe/data/interface.h"

namespace fe
{
	class ast;
}

namespace fe::ext_ast
{
	fe::interface extract_interface(ast &ast);
}
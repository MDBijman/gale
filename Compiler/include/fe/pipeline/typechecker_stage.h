#pragma once
#include "fe/data/type_scope.h"

namespace fe
{
	struct interface;
	using interfaces = std::vector<interface>;
}

namespace fe::ext_ast
{
	class ast;
	struct node;

	types::unique_type typeof(node& n, ast& ast, type_constraints tc = type_constraints());
	void typecheck(ast& ast, const interfaces& ifaces);
}

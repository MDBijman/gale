#pragma once

namespace fe::ext_ast
{
	struct node;
	class ast;

	void resolve(node& n, ast& ast);
}
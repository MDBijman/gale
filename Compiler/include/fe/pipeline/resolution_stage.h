#pragma once
#include <vector>

namespace fe
{
	struct interface;
	using interfaces = std::vector<interface>;
}

namespace fe::ext_ast
{
	struct node;
	class ast;

	void resolve(ast &ast, const interfaces &proj_ifaces);
} // namespace fe::ext_ast
#include "ast.h"

namespace ast
{
	void node::add_child(node * node)
	{
		children.push_back(node);
	}
}

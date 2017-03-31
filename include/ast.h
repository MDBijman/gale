#pragma once
#include <vector>

namespace ast
{
	class node
	{
	public:
		void add_child(node* node);
		std::vector<node*> children;
	};
}

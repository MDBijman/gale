#pragma once
#include <vector>

namespace ast
{
	template <class T>
	class node
	{
	public:
		node(T t) : t(t) {}
		~node() 
		{ 
			for (node* node : children)
				delete node;
		}

		void add_child(node* node)
		{
			children.push_back(node);
		}

		bool is_leaf()
		{
			return children.empty();
		}

		std::vector<node*> children;
		T t;
	};
}

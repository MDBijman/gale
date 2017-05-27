#pragma once
#include <vector>

namespace ast
{
	template <class T>
	class node
	{
	public:
		node(T value) : value(value) {}
		~node()
		{
			for (auto child : children)
				delete child;
		}

		void add_child(node<T>* node)
		{
			children.push_back(node);
		}

		bool is_leaf()
		{
			return children.empty();
		}

		std::vector<node<T>*> children;
		T value;
	};
}

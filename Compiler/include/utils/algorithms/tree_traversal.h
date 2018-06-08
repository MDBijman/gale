#pragma once

namespace utils::algorithms
{
	template<class T>
	static std::vector<typename T::index> breadth_first(T tree)
	{
		std::vector<T::index> result;
		result.reserve(tree.size());

		std::vector<T::index> todo;

		todo.push_back(tree.root());
		while (!todo.empty())
		{
			auto next = todo.back();
			todo.pop_back();

			result.push_back(next);

			if (tree.is_leaf(next))
				continue;

			auto& children = tree.children_of(next);

			for (auto it = children.rbegin(); it != children.rend(); it++)
				todo.push_back(*it);
		}

		return result;
	}

	template<class T>
	static std::vector<typename T::index> post_order(T& tree)
	{
		// This is the vector that holds the final result, which is the tree nodes in post order
		std::vector<T::index> result;
		result.reserve(tree.size());

		// Post order traversal of the tree 
		// This is a vector instead of stack so that we take elements off in the right order, without needing to reverse
		std::vector<T::index> converted;

		// Stack of nodes being processed, vector for speed
		std::vector<T::index> s;
		s.push_back(tree.root());
		// Stack of offsets of nodes being processed in their parents' child list
		std::vector<T::index> o;
		o.push_back(0);

		T::index last_visited = -1;

		while (!s.empty())
		{
			auto x = s.back();
			auto x_offset = o.back();

			// If we have a non terminal node, we 'recursively' process it
			if (!tree.is_leaf(x))
			{
				auto& children = tree.children_of(x);

				// If the curr_node has no children, we just put it onto the converted stack
				if (children.size() == 0)
				{
					// visit
					result.push_back(x);
					last_visited = x;
					s.pop_back();
					continue;
				}

				// If we have children, we check if we can find the last_visited node in the curr_node children
				// There are 3 cases

				if (last_visited != -1 && tree.parent_of(last_visited) == x)
				{
					// Case 1
					// last_visited is the last of the curr_node children, so we have visited all children
					if (x_offset == children.size() - 1)
					{
						s.pop_back();
						o.pop_back();
						result.push_back(x);
						last_visited = x;
					}
					// Case 2
					// last_visited is an element of the curr_node children, but not the last
					// Push its sibling onto the stack
					// In the next iteration(s) this node is further processed
					else
					{
						s.push_back(children[x_offset + 1]);
						o.back() = x_offset + 1;
					}
				}
				// Case 3
				// last_visited is not part of the curr_node children, so we push the leftmost child onto the stack
				else
				{
					// Push left
					s.push_back(*children.begin());
					o.push_back(0);
				}
			}
			// If we have a terminal node, we just put it onto the converted stack
			else
			{
				result.push_back(x);
				last_visited = x;
				s.pop_back();
			}
		}

		return result;
	}
}
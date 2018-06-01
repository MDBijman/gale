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

			auto& children = tree.get_children_of(next);

			for (auto it = children.rbegin(); it != children.rend(); it++)
				todo.push_back(*it);
		}

		return result;
	}

	template<class T>
	static std::vector<typename T::index> post_order(T tree)
	{
		// This is the vector that holds the final result, which is the tree nodes in post order
		std::vector<T::index> result;
		result.reserve(tree.size());

		// Post order traversal of the tree 
		// This is a vector instead of stack so that we take elements off in the right order, without needing to reverse
		std::vector<T::index> converted;

		// Get bottom left child
		std::stack<T::index> s;
		s.push(tree.root());

		T::index last_visited;

		while (!s.empty())
		{
			auto x = s.top();

			// If we have a non terminal node, we 'recursively' process it
			if (!tree.is_leaf(x))
			{
				auto& children = tree.get_children_of(x);

				// If the curr_node has no children, we just put it onto the converted stack
				if (children.size() == 0)
				{
					// visit
					result.push_back(x);
					converted.push_back(x);
					last_visited = x;
					s.pop();
					continue;
				}

				// If we have children, we check if we can find the last_visited node in the curr_node children
				// There are 3 cases: not found, found as the last child, found before the last child

				auto it = std::find(children.begin(), children.end(), last_visited);

				// Case 1
				// last_visited is not part of the curr_node children, so we push the leftmost child onto the stack
				if (it == children.end())
				{
					// Push left
					auto next = *children.begin();
					s.push(next);
				}
				// Case 2 
				// last_visited is the last of the curr_node children, so we have visited all children
				// We construct an ebnf non_terminal from the already converted children that are on the converted stack
				// They are transformed if they are a a result of a repetition/optional/group rule
				else if (it == children.end() - 1)
				{
					s.pop();
					result.push_back(x);
					converted.resize(converted.size() - children.size());
					converted.push_back(x);
					last_visited = x;
				}
				// Case 3 
				// last_visited is an element of the curr_node children, but not the last
				// Push its sibling onto the stack
				// In the next iteration(s) this node is further processed
				else
				{
					auto next = *(it + 1);
					s.push(next);
				}
			}
			// If we have a terminal node, we just put it onto the converted stack
			else
			{
				result.push_back(x);
				last_visited = x;
				s.pop();
				converted.push_back(last_visited);
			}
		}

		return result;
	}
}
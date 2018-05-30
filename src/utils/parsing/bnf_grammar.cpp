#include "utils/parsing/bnf_grammar.h"
#include <stack>

namespace utils::bnf
{
	bool operator==(const rule& r1, const rule& r2)
	{
		return r1.first == r2.first && (r1.second == r2.second);
	}

	// #todo make handling variants easier
	void destroy_node(std::variant<terminal_node, non_terminal_node>* n)
	{
		// This is the same post order iteration algorithm as used for bnf to ebnf parse tree conversion
		std::stack<std::variant<non_terminal_node*, terminal_node*>> s;
		std::variant<non_terminal_node*, terminal_node*> last_visited;
		if (std::holds_alternative<terminal_node>(*n))
			s.push(&std::get<terminal_node>(*n));
		else if (std::holds_alternative<non_terminal_node>(*n))
			s.push(&std::get<non_terminal_node>(*n));

		std::vector<std::variant<non_terminal_node*, terminal_node*>> delete_queue;

		while (!s.empty())
		{
			auto x = s.top();
			if (std::holds_alternative<non_terminal_node*>(x))
			{
				non_terminal_node* curr_node = std::get<non_terminal_node*>(x);

				if (curr_node->children.size() == 0)
				{
					last_visited = curr_node;
					delete_queue.push_back(curr_node);
					s.pop();
					continue;
				}

				// Check if last visited is part of children
				decltype(curr_node->children.begin()) it;
				for (it = curr_node->children.begin(); it != curr_node->children.end(); it++)
				{
					auto& child = *(it->get());
					if ((std::holds_alternative<non_terminal_node>(child)
						&& std::holds_alternative<non_terminal_node*>(last_visited)
						&& (&std::get<non_terminal_node>(child) == std::get<non_terminal_node*>(last_visited)))
						|| (std::holds_alternative<terminal_node>(child)
							&& std::holds_alternative<terminal_node*>(last_visited)
							&& (&std::get<terminal_node>(child) == std::get<terminal_node*>(last_visited))))
					{
						break;
					}
				}

				// If it is not a part, visit the leftmost child
				if (it == curr_node->children.end())
				{
					// Push left
					auto next = curr_node->children.begin()->get();
					if (std::holds_alternative<non_terminal_node>(*next))
						s.push(&std::get<non_terminal_node>(*next));
					else if (std::holds_alternative<terminal_node>(*next))
						s.push(&std::get<terminal_node>(*next));
				}
				// If it is the last child, visit this node itself
				else if (it == curr_node->children.end() - 1)
				{
					s.pop();
					last_visited = curr_node;
					delete_queue.push_back(curr_node);
				}
				// Else if it is somewhere in the middle, visit the next child
				else
				{
					auto next = (it + 1)->get();
					if (std::holds_alternative<non_terminal_node>(*next))
						s.push(&std::get<non_terminal_node>(*next));
					else if (std::holds_alternative<terminal_node>(*next))
						s.push(&std::get<terminal_node>(*next));
				}
			}
			else
			{
				last_visited = std::get<terminal_node*>(x);
				delete_queue.push_back(last_visited);
				s.pop();
			}
		}

		for (auto& elem : delete_queue)
		{
			if (std::holds_alternative<non_terminal_node*>(elem))
				free(std::get<non_terminal_node*>(elem));
			else if (std::holds_alternative<terminal_node*>(elem))
				free(std::get<terminal_node*>(elem));
		}
	}
}
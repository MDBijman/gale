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
		// A version with more comments can be found in ebnf_parser.cpp
		std::stack<std::variant<bnf::non_terminal_node*, bnf::terminal_node*>> s;
		std::variant<bnf::non_terminal_node*, bnf::terminal_node*> last_visited;
		if (std::holds_alternative<terminal_node>(*n))
			s.push(&std::get<terminal_node>(*n));
		else if (std::holds_alternative<non_terminal_node>(*n))
			s.push(&std::get<non_terminal_node>(*n));

		while (!s.empty())
		{
			auto x = s.top();
			if (std::holds_alternative<bnf::non_terminal_node*>(x))
			{
				bnf::non_terminal_node* curr_node = std::get<bnf::non_terminal_node*>(x);

				if (curr_node->children.size() == 0)
				{
					free(curr_node);
					last_visited = curr_node;
					s.pop();
					continue;
				}

				decltype(curr_node->children.begin()) it;
				for (it = curr_node->children.begin(); it != curr_node->children.end(); it++)
				{
					auto& child = *(it->get());
					if ((std::holds_alternative<bnf::non_terminal_node>(child)
						&& std::holds_alternative<bnf::non_terminal_node*>(last_visited)
						&& (&std::get<bnf::non_terminal_node>(child) == std::get<bnf::non_terminal_node*>(last_visited)))
						|| (std::holds_alternative<bnf::terminal_node>(child)
							&& std::holds_alternative<bnf::terminal_node*>(last_visited)
							&& (&std::get<bnf::terminal_node>(child) == std::get<bnf::terminal_node*>(last_visited))))
					{
						break;
					}
				}

				if (it == curr_node->children.end())
				{
					// Push left
					auto next = curr_node->children.begin()->get();
					if (std::holds_alternative<bnf::non_terminal_node>(*next))
						s.push(&std::get<bnf::non_terminal_node>(*next));
					else if (std::holds_alternative<bnf::terminal_node>(*next))
						s.push(&std::get<bnf::terminal_node>(*next));
				}
				else if (it == curr_node->children.end() - 1)
				{
					s.pop();
					last_visited = curr_node;
					free(curr_node);
				}
				else
				{
					auto next = (it + 1)->get();
					if (std::holds_alternative<bnf::non_terminal_node>(*next))
						s.push(&std::get<bnf::non_terminal_node>(*next));
					else if (std::holds_alternative<bnf::terminal_node>(*next))
						s.push(&std::get<bnf::terminal_node>(*next));
				}
			}
			else
			{
				free(std::get<bnf::terminal_node*>(x));
				last_visited = std::get<bnf::terminal_node*>(x);
				s.pop();
			}
		}
	}
}
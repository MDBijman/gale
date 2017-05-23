#include "bnf_parser.h"
#include <stack>

namespace language 
{
	namespace bnf
	{
		ast::node<symbol>* parser::parse(non_terminal initial, std::vector<terminal> input)
		{
			input.push_back(end_of_input);

			std::stack<ast::node<symbol>*> stack;
			ast::node<symbol>* root = new ast::node<symbol>(initial);
			stack.push(root);

			this->parse(stack, input);
			this->prune(root);

			return root;
		}

		void parser::parse(std::stack<ast::node<symbol>*>& stack, std::vector<terminal>& input) const
		{
			auto it = input.begin();
			while (!stack.empty())
			{
				if (it == input.end())
				{
					throw std::runtime_error("Unexpected end of input.");
				}

				auto node = stack.top();
				auto top_symbol = node->t;

				auto input_token = *it;

				if (top_symbol.is_terminal())
				{
					if (input_token == top_symbol.get_terminal())
					{
						std::cout << "Matching: " << (int)input_token << std::endl;

						node->add_child(new ast::node<symbol>(input_token));
						it++;
						stack.pop();
					}
					else if (top_symbol.get_terminal() == epsilon)
					{
						stack.pop();
					}
					else
						throw std::runtime_error(std::string("Unexpected terminal: ").append(std::to_string((int)input_token)).append(" when matching: ").append(std::to_string((int)top_symbol.get_terminal())));
				}
				else
				{
					auto non_terminal_symbol = top_symbol.get_non_terminal();
					if (!rules.has_rule(non_terminal_symbol))
						throw std::runtime_error(std::string("Unable to unify, unrecognized non-terminal: ").append(std::to_string((int)input_token)));

					auto rule_rhs = rules.match(non_terminal_symbol, input_token);

					// Pop non terminal off the stack
					stack.pop();

					// Push the symbols onto the stack
					for (auto rule_it = rule_rhs.rbegin(); rule_it != rule_rhs.rend(); rule_it++)
					{
						ast::node<symbol>* new_symbol = new ast::node<symbol>(*rule_it);
						node->add_child(new_symbol);
						stack.push(new_symbol);
					}
				}
			}
		}

		void parser::prune(ast::node<symbol>* tree) const
		{
			for (auto child : tree->children)
			{
				this->prune(child);
			}

			tree->children.erase(
				std::remove_if(tree->children.begin(), tree->children.end(), [](auto& child) {
				if (child->t.is_terminal() && (child->t.get_terminal() == epsilon) ||
					(!child->t.is_terminal() && child->is_leaf()))
				{
					delete child;
					return true;
				}
				return false;
			}),
				tree->children.end()
				);
		}
	}
}
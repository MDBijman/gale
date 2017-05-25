#include "bnf_parser.h"
#include <stack>
#include <variant>

namespace language 
{
	namespace bnf
	{
		std::variant<ast::node<symbol>*, error> parser::parse(non_terminal begin_symbol, std::vector<terminal> input) const
		{
			input.push_back(end_of_input);
			std::stack<ast::node<symbol>*> stack;
			auto root = new ast::node<symbol>(begin_symbol);
			stack.push(root);

			auto it = input.begin();
			while (!stack.empty())
			{
				if (it == input.end())
					return error{ error_code::UNEXPECTED_END_OF_INPUT, std::string("Encountered eoi token with non-empty stack") };

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
						return error{ error_code::TERMINAL_MISMATCH, std::string("Got: ").append(std::to_string((int)input_token)).append(" Expected: ").append(std::to_string((int)top_symbol.get_terminal())) };
				}
				else
				{
					auto non_terminal_symbol = top_symbol.get_non_terminal();
					if (!rules.has_rule(non_terminal_symbol))
						return error{ error_code::UNEXPECTED_NON_TERMINAL, std::to_string((int) input_token) };

					auto rhs_or_error = rules.match(non_terminal_symbol, input_token);
					if (std::holds_alternative<error>(rhs_or_error))
						return std::get<error>(rhs_or_error);
					auto rule_rhs = std::get<const std::vector<symbol>*>(rhs_or_error);

					// Pop non terminal off the stack
					stack.pop();

					// Push the symbols onto the stack
					for (auto rule_it = rule_rhs->rbegin(); rule_it != rule_rhs->rend(); rule_it++)
					{
						ast::node<symbol>* new_symbol = new ast::node<symbol>(*rule_it);
						node->add_child(new_symbol);
						stack.push(new_symbol);
					}
				}
			}

			this->prune(root);
			return root;
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
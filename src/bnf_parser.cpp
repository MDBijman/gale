#include "bnf_parser.h"
#include <stack>
#include <variant>

namespace language 
{
	namespace bnf
	{
		std::variant<node*, error> parser::parse(non_terminal begin_symbol, std::vector<terminal> input) const
		{
			input.push_back(end_of_input);
			std::stack<node*> stack;
			auto root = new node(begin_symbol);
			stack.push(root);

			auto it = input.begin();
			while (!stack.empty())
			{
				if (it == input.end())
					return error{ error_code::UNEXPECTED_END_OF_INPUT, std::string("Encountered eoi token with non-empty stack") };

				node* top = stack.top();

				auto input_token = *it;

				if (top->value.is_terminal())
				{
					if (input_token == top->value.get_terminal())
					{
						top->children.push_back(new node(input_token));
						it++;
						stack.pop();
					}
					else if (top->value.get_terminal() == epsilon)
					{
						stack.pop();
					}
					else
						return error{ error_code::TERMINAL_MISMATCH, std::string("Got: ").append(std::to_string((int)input_token)).append(" Expected: ").append(std::to_string((int)top->value.get_terminal())) };
				}
				else
				{
					auto non_terminal_symbol = top->value.get_non_terminal();

					auto rhs_or_error = match(non_terminal_symbol, input_token);
					if (std::holds_alternative<error>(rhs_or_error))
						return std::get<error>(rhs_or_error);
					auto rule_rhs = std::get<const std::vector<symbol>*>(rhs_or_error);

					// Pop non terminal off the stack
					stack.pop();

					// Push the symbols onto the stack
					for (auto rule_it = rule_rhs->rbegin(); rule_it != rule_rhs->rend(); rule_it++)
					{
						node* new_symbol = new node(*rule_it);
						top->children.insert(top->children.begin(), new_symbol);
						stack.push(new_symbol);
					}
				}
			}

			return root;
		}

	
	}
}
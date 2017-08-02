#include "bnf_parser.h"
#include <stack>
#include <variant>

namespace tools 
{
	namespace bnf
	{
		std::variant<std::unique_ptr<node>, error> parser::parse(non_terminal begin_symbol, std::vector<terminal_node> input) const
		{
			input.push_back({ end_of_input, "" });
			std::stack<node*> stack;
			auto root = std::make_unique<node>(non_terminal_node(begin_symbol));
			stack.push(root.get());

			auto it = input.begin();
			while (!stack.empty())
			{
				if (it == input.end())
					return error{ error_code::UNEXPECTED_END_OF_INPUT, std::string("Encountered eoi token with non-empty stack") };

				node* top = stack.top();

				auto input_token = *it;

				if (std::holds_alternative<terminal_node>(*top))
				{
					auto value = std::get<terminal_node>(*top).value;

					if (input_token.value == value)
					{
						std::get<terminal_node>(*top).token = input_token.token;
						it++;
						stack.pop();
					}
					else if (value == epsilon)
					{
						stack.pop();
					}
					else
					{
						return error{ error_code::TERMINAL_MISMATCH, 
							std::string("Got: ")
							.append(std::to_string((int)input_token.value))
							.append(" Expected: ")
							.append(std::to_string((int)value))
							.append(" at token index ")
							.append(std::to_string(std::distance(input.begin(), it)))
						};
					}
				}
				else
				{
					auto& nt = std::get<non_terminal_node>(*top);

					auto rhs_or_error = match(nt.value, input_token.value);
					if (std::holds_alternative<error>(rhs_or_error))
						return std::get<error>(rhs_or_error);
					auto rule_rhs = std::get<const std::vector<symbol>*>(rhs_or_error);

					// Pop non terminal off the stack
					stack.pop();

					// Push the symbols onto the stack
					for (auto rule_it = rule_rhs->rbegin(); rule_it != rule_rhs->rend(); rule_it++)
					{
						std::unique_ptr<node> new_symbol;
						if ((*rule_it).is_terminal())
							new_symbol = std::make_unique<node>(terminal_node(rule_it->get_terminal(), ""));
						else
							new_symbol = std::make_unique<node>(non_terminal_node(rule_it->get_non_terminal()));
						stack.push(new_symbol.get());
						nt.children.insert(nt.children.begin(), std::move(new_symbol));
					}
				}
			}

			return root;
		}
	}
}
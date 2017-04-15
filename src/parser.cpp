#include "parser.h"

namespace ebnf
{
	ast::node<symbol>* parser::parse(std::vector<terminal> input)
	{
		input.push_back(terminal::END_OF_INPUT);

		ast::node<symbol>* root = new ast::node<symbol>(non_terminal::RULESET);
		stack.push(root);

		auto it = input.begin();
		while (!stack.empty())
		{
			if (it == input.end())
			{
				throw std::runtime_error("Unexpected end of input.");
			}

			auto node = stack.top();
			auto symbol = node->t;

			auto input_token = *it;

			if (symbol.is_terminal())
			{
				if (input_token == symbol.get_terminal())
				{
					std::cout << "Matching: " << (int)input_token << std::endl;

					node->add_child(new ast::node<ebnf::symbol>(input_token));
					it++;
					stack.pop();
				}
				else if (symbol.get_terminal() == terminal::EPSILON)
				{
					stack.pop();
				}
				else
					throw std::runtime_error(std::string("Unexpected terminal: ").append(std::to_string((int)input_token)).append(" when matching: ").append(std::to_string((int)symbol.get_terminal())));
			}
			else
			{
				auto non_terminal_symbol = symbol.get_non_terminal();
				if (mapping.find(non_terminal_symbol) == mapping.end()) 
					throw std::runtime_error(std::string("Unable to unify, unrecognized non-terminal: ").append(std::to_string((int)input_token)));

				auto rules = mapping.at(non_terminal_symbol);

				// Find a rule that matches
				auto rule_location = std::find_if(rules.begin(), rules.end(), [&](auto& rule) { return rule.at(0).matches(input_token, mapping); });
				if (rule_location == rules.end())
					throw std::runtime_error(std::string("Unable to unify, no matching rule: ").append(std::to_string((int)input_token)));
				auto rule = *rule_location;

				// Pop non terminal off the stack
				stack.pop();

				// Push the symbols onto the stack
				for (auto rule_it = rule.rbegin(); rule_it != rule.rend(); rule_it++)
				{
					ast::node<ebnf::symbol>* new_symbol = new ast::node<ebnf::symbol>(*rule_it);
					node->add_child(new_symbol);
					stack.push(new_symbol);
				}
			}
		}

		return root;
	}
}
#include "bnf_parser.h"
#include <stack>
#include <variant>

namespace tools::bnf
{
	std::variant<const std::vector<symbol>*, error> parser::match(non_terminal lhs, terminal input_token) const
	{
		// All rules with the given non terminal on the lhs
		auto possible_matches = rules.equal_range(lhs);

		if (possible_matches.first == possible_matches.second)
			return error{ error_code::NO_MATCHING_RULE, std::to_string((int)input_token) };

		// Set to true if the symbol can be the empty string
		const std::vector<symbol>* null_rule = nullptr;

		// Find a rule that matches
		auto rule_location = std::find_if(possible_matches.first, possible_matches.second, [&](auto& rule) {
			auto& first_on_rhs = rule.second.at(0);

			if (first_on_rhs.is_terminal() && (first_on_rhs.get_terminal() == epsilon))
			{
				null_rule = &rule.second;
				return false;
			}

			return first_on_rhs.matches(input_token, rules);
		});

		if (rule_location == possible_matches.second)
		{
			if (null_rule)
			{
				return null_rule;
			}
			else
			{
				return error{
					error_code::NO_MATCHING_RULE,
					std::string("No matching rule for lhs ")
						.append(std::to_string(lhs))
						.append(" attempted to match ")
						.append(std::to_string(input_token))
				};
			}
		}

		return &rule_location->second;
	}

	std::variant<std::unique_ptr<node>, error> parser::parse(non_terminal begin_symbol, std::vector<terminal_node> input)
	{
		generate_table();
		table_is_old = false;

		input.push_back({ end_of_input, "" });
		std::stack<node*> stack;
		auto root = std::make_unique<node>(non_terminal_node(begin_symbol));
		stack.push(root.get());

		auto distance = 0;
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
					return error{
						error_code::TERMINAL_MISMATCH,
						std::string("Got: ")
						.append(std::to_string((int)input_token.value))
						.append(" Expected: ")
						.append(std::to_string((int)value))
						.append(" at token index ")
						.append(std::to_string(distance))
					};
				}

				distance += input_token.token.size();
			}
			else
			{
				auto& nt = std::get<non_terminal_node>(*top);

				auto rule_rhs = table.at({ nt.value, input_token.value })->second;

				// Pop non terminal off the stack
				stack.pop();

				// Push the symbols onto the stack
				for (auto rule_it = rule_rhs.rbegin(); rule_it != rule_rhs.rend(); rule_it++)
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

	void parser::generate_table()
	{
		if (!table_is_old) return;

		std::unordered_map<non_terminal, std::unordered_set<std::variant<terminal, non_terminal>>> first_sets;
		std::unordered_map<non_terminal, std::unordered_set<std::variant<terminal, non_terminal>>> following_sets;

		for (const auto& rule : rules)
		{
			// If first set doesnt exist yet make one
			if (auto loc = first_sets.find(rule.first); loc == first_sets.end())
				first_sets.insert({ rule.first, std::unordered_set<std::variant<terminal, non_terminal>>() });
			first_sets.at(rule.first).insert(rule.second.at(0).value);

			// If following set doesnt exist yet make one
			if (auto loc = following_sets.find(rule.first); loc == following_sets.end())
				following_sets.insert({ rule.first, std::unordered_set<std::variant<terminal, non_terminal>>() });
		}

		following_sets.at(1).insert(end_of_input);

		// Generate first sets
		bool changing = true;
		while (changing)
		{
			changing = false;

			for (auto& first_set : first_sets)
			{
				
				// For each non terminal A in the first sets, add the first set of A to the set
				std::vector<non_terminal> non_terminals;
				for (auto& element : first_set.second)
				{
					if (std::holds_alternative<non_terminal>(element))
					{
						auto nt = std::get<non_terminal>(element);
						if (nt == first_set.first) throw std::runtime_error("Left recursion");
						non_terminals.push_back(nt);
						changing = true;
					}
				}

				for (auto nt : non_terminals) first_set.second.erase(nt);
				for (auto nt : non_terminals) first_set.second.insert(first_sets.at(nt).begin(), first_sets.at(nt).end());
			}
		}

		// Generate following sets
		changing = true;
		while (changing)
		{
			changing = false;

			for (const auto& rule : rules)
			{
				for (auto it = rule.second.begin(); it != rule.second.end(); it++)
				{
					if (it->is_terminal()) continue;

					auto nt = it->get_non_terminal();
					auto& first_set = first_sets.at(nt);
					auto& following_set = following_sets.at(nt);

					// For each production X -> alphaA put FOLLOW(X) in FOLLOW(A)
					if (it == rule.second.end() - 1)
					{
						auto pre = following_set.size();
						following_set.insert(following_sets.at(rule.first).begin(), following_sets.at(rule.first).end());
						if (pre != following_set.size()) changing = true;
						continue;
					}

					// Put FIRST(beta) - {epsilon} in FOLLOW(A)
					auto next_symbol = it + 1;
					if (next_symbol->is_terminal())
					{
						following_set.insert(next_symbol->get_terminal());
					}
					else
					{
						auto next_nt = next_symbol->get_non_terminal();
						auto& next_first_set = first_sets.at(next_nt);

						auto pre = following_sets.at(nt).size();
						if (auto it = std::find(next_first_set.begin(), next_first_set.end(), std::variant<terminal, non_terminal>(terminal(epsilon))); it != next_first_set.end())
						{
							// If epsilon is in FIRST(beta) then put FOLLOW(X) in FOLLOW(A)
							following_set.insert(following_sets.at(rule.first).begin(), following_sets.at(rule.first).end());
						}

						// Insert all and remove epsilon
						following_set.insert(next_first_set.begin(), next_first_set.end());
						following_set.erase(epsilon);

						if (pre != following_set.size()) changing = true;
					}
				}
			}
		}

		for (auto it = rules.begin(); it != rules.end(); it++)
		{
			auto first = it->second.at(0);

			if (first.is_terminal())
			{
				// If epsilon is in First(rhs) then for each terminal t in Follow(lhs) put alpha in Table[lhs, t]
				if (first.get_terminal() == epsilon)
				{
					for (const auto& follower : following_sets.at(it->first))
					{
						auto [_, inserted] = table.insert({ { it->first, std::get<terminal>(follower) }, it });
						if (!inserted) throw std::runtime_error("Conflict");
					}
				}
				else
				{
					auto [_, inserted] = table.insert({ { it->first, first.get_terminal() }, it });
					if (!inserted) throw std::runtime_error("Conflict");
				}
			}
			else
			{
				auto nt = first.get_non_terminal();
				auto& first_set = first_sets.at(nt);

				bool has_epsilon = false;
				for (auto& symbol : first_set)
				{
					if (std::get<terminal>(symbol) == epsilon)
					{
						has_epsilon = true;
						continue;
					}

					auto[conflict, inserted] = table.insert({ {it->first, std::get<terminal>(symbol)}, it });
					if (!inserted) throw std::runtime_error("Conflict");
				}

				if (has_epsilon)
				{
					auto& follow_set = following_sets.at(it->first);

					for (auto& symbol : follow_set)
					{
						auto[conflict, inserted] = table.insert({ { it->first, std::get<terminal>(symbol) }, it });
						if (!inserted) throw std::runtime_error("Conflict");
					}
				}
			}
		}
	}
}
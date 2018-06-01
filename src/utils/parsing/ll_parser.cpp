#include "utils/parsing/ll_parser.h"

#include <stack>
#include <variant>

namespace utils::ll
{
	bnf::tree parser::parse(std::vector<bnf::terminal_node> input)
	{
		bnf::tree t;

		input.push_back({ bnf::end_of_input, "" });
		std::stack<bnf::node_id> stack;
		stack.push(t.create_non_terminal(bnf::non_terminal_node(start_symbol, {})));

		std::size_t distance = 0;
		auto it = input.begin();
		while (!stack.empty())
		{
			if (it == input.end())
				throw error{ error_code::UNEXPECTED_END_OF_INPUT, std::string("Encountered eoi token with non-empty stack") };
			auto input_token = *it;

			bnf::node_id top = stack.top();
			auto& node = t.get_node(top);

			if (node.kind == bnf::node_type::TERMINAL)
			{
				auto& data = t.get_terminal(node.value_id);

				if (data.first == input_token.first)
				{
					data.second = input_token.second;
					it++;
					stack.pop();
				}
				else if (data.first == bnf::epsilon)
				{
					stack.pop();
				}
				else
				{
					throw error{
						error_code::TERMINAL_MISMATCH,
						std::string("Got: ")
						.append(std::to_string((int)input_token.first))
						.append(" Expected: ")
						.append(std::to_string((int)data.first))
						.append(" at token index ")
						.append(std::to_string(distance))
					};
				}

				distance += input_token.second.size();
			}
			else
			{
				auto& data = t.get_non_terminal(node.value_id);

				auto rule_rhs = table.at({ data.first, input_token.first })->second;

				// Pop non terminal off the stack
				stack.pop();

				// Push the symbols onto the stack
				for (auto rule_it = rule_rhs.rbegin(); rule_it != rule_rhs.rend(); rule_it++)
				{
					if (rule_it->is_terminal())
					{
						auto new_t = t.create_terminal(bnf::terminal_node(rule_it->get_terminal(), ""));
						stack.push(new_t);
						data.second.insert(data.second.begin(), new_t);
					}
					else
					{
						auto new_nt = t.create_non_terminal(bnf::non_terminal_node(rule_it->get_non_terminal(), {}));
						stack.push(new_nt);
						data.second.insert(data.second.begin(), new_nt);
					}

				}
			}
		}

		return t;
	}

	void parser::generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& rules)
	{
		this->start_symbol = start_symbol;

		std::unordered_map<bnf::non_terminal, std::unordered_set<std::variant<bnf::terminal, bnf::non_terminal>>> first_sets;
		std::unordered_map<bnf::non_terminal, std::unordered_set<std::variant<bnf::terminal, bnf::non_terminal>>> following_sets;

		for (const auto& rule : rules)
		{
			// If first set doesnt exist yet make one
			if (auto loc = first_sets.find(rule.first); loc == first_sets.end())
				first_sets.insert({ rule.first, std::unordered_set<std::variant<bnf::terminal, bnf::non_terminal>>() });
			first_sets.at(rule.first).insert(rule.second.at(0).value);

			// If following set doesnt exist yet make one
			if (auto loc = following_sets.find(rule.first); loc == following_sets.end())
				following_sets.insert({ rule.first, std::unordered_set<std::variant<bnf::terminal, bnf::non_terminal>>() });
		}

		following_sets.at(1).insert(bnf::end_of_input);

		// Generate first sets
		bool changing = true;
		while (changing)
		{
			changing = false;

			for (auto& first_set : first_sets)
			{

				// For each non terminal A in the first sets, add the first set of A to the set
				std::vector<bnf::non_terminal> non_terminals;
				for (auto& element : first_set.second)
				{
					if (std::holds_alternative<bnf::non_terminal>(element))
					{
						auto nt = std::get<bnf::non_terminal>(element);
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
						if (auto it = std::find(next_first_set.begin(), next_first_set.end(), std::variant<bnf::terminal, bnf::non_terminal>(bnf::terminal(bnf::epsilon))); it != next_first_set.end())
						{
							// If epsilon is in FIRST(beta) then put FOLLOW(X) in FOLLOW(A)
							following_set.insert(following_sets.at(rule.first).begin(), following_sets.at(rule.first).end());
						}

						// Insert all and remove epsilon
						following_set.insert(next_first_set.begin(), next_first_set.end());
						following_set.erase(bnf::epsilon);

						if (pre != following_set.size()) changing = true;
					}
				}
			}
		}

		// Generate table
		for (auto it = rules.begin(); it != rules.end(); it++)
		{
			auto first = it->second.at(0);

			if (first.is_terminal())
			{
				// If epsilon is in First(rhs) then for each terminal t in Follow(lhs) put alpha in Table[lhs, t]
				if (first.get_terminal() == bnf::epsilon)
				{
					for (const auto& follower : following_sets.at(it->first))
					{
						auto[_, inserted] = table.insert({ { it->first, std::get<bnf::terminal>(follower) }, it });
						if (!inserted) throw std::runtime_error("Conflict");
					}
				}
				else
				{
					auto[_, inserted] = table.insert({ { it->first, first.get_terminal() }, it });
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
					if (std::get<bnf::terminal>(symbol) == bnf::epsilon)
					{
						has_epsilon = true;
						continue;
					}

					auto[conflict, inserted] = table.insert({ {it->first, std::get<bnf::terminal>(symbol)}, it });
					if (!inserted) throw std::runtime_error("Conflict");
				}

				if (has_epsilon)
				{
					auto& follow_set = following_sets.at(it->first);

					for (auto& symbol : follow_set)
					{
						auto[conflict, inserted] = table.insert({ { it->first, std::get<bnf::terminal>(symbol) }, it });
						if (!inserted) throw std::runtime_error("Conflict");
					}
				}
			}
		}
	}
}
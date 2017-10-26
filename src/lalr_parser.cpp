#include "lalr_parser.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>

namespace tools::lalr
{
	void parser::generate_item_sets()
	{
		item_sets.push_back(item_set());

		auto expand_item_set = [&](item_set& i) {
			std::size_t index = 0;
			for (; index < i.items.size(); index++)
			{
				const auto& set_item = i.items.at(index);

				if (set_item.bullet_offset >= set_item.rule.rhs.size())
					continue;

				auto expected_symbol = set_item.rule.rhs.at(set_item.bullet_offset);
				if (expected_symbol.is_terminal())
					continue;

				// TODO multiple rules for one nt dont work
				// Bullet is not too far right and the symbol after it is a non terminal

				for (
					auto it = std::find_if(rules.begin(), rules.end(), [&](const bnf::rule& r) { return r.lhs == expected_symbol.get_non_terminal(); });
					it != rules.end();
					it = std::find_if(++it, rules.end(), [&](const bnf::rule& r) { return r.lhs == expected_symbol.get_non_terminal(); })
					)
				{
					auto new_item = item{ *it, 0 };
				
					if(std::find(i.items.begin(), i.items.end(), new_item) == i.items.end())
						i.items.push_back(new_item);
				}
			}
		};

		auto create_item_sets = [&](item_set& i) {
			std::size_t position = std::find(item_sets.begin(), item_sets.end(), i) - item_sets.begin();

			std::vector<std::pair<bnf::symbol, item_set>> new_sets;
			for (const auto& item : i.items)
			{
				// Filter out rules that have no expected symbols left
				if (item.bullet_offset >= item.rule.rhs.size())
					continue;

				// Get the expected symbol of the current item
				auto sym = item.expected_symbol();
				// And create a new item set if it doesn't exist yet in this iteration
				if (std::find_if(new_sets.begin(), new_sets.end(), [sym](const auto& pair) { return pair.first == sym; }) == new_sets.end())
					new_sets.push_back({ sym, item_set() });

				// Create new item with dot shifted right
				auto next_item = item;
				next_item.bullet_offset++;

				// Insert new item in the item set
				std::find_if(new_sets.begin(), new_sets.end(), [sym](const auto& pair) { return pair.first == sym; })->second.items.push_back(next_item);
			}

			// Create item set
			for (auto& pair : new_sets)
			{
				auto[sym, set] = pair;

				expand_item_set(set);

				// Check if the item set already exists
				auto pos = std::find(item_sets.begin(), item_sets.end(), set);
				// If not, add it
				if (pos == item_sets.end())
				{
					item_sets.push_back(set);
					transitions.push_back({ position, item_sets.size() - 1, sym });
				}
				// In both cases add a transition
				else
				{
					transitions.push_back({ position, static_cast<std::size_t>(pos - item_sets.begin()), sym });
				}
			}
		};

		auto create_first_follow_sets = [&]() {
			for (const auto& rule : rules)
			{
				// If first set doesnt exist yet make one
				if (auto loc = first.find(rule.lhs); loc == first.end())
					first.insert({ rule.lhs, std::unordered_set<bnf::symbol>() });
				if(!(rule.rhs.at(0) == rule.lhs))
					first.at(rule.lhs).insert(rule.rhs.at(0).value);

				// If following set doesnt exist yet make one
				if (auto loc = follow.find(rule.lhs); loc == follow.end())
					follow.insert({ rule.lhs, std::unordered_set<bnf::symbol>() });
			}
		};

		auto generate_first_sets = [&]() {
			bool changing = true;
			while (changing)
			{
				changing = false;

				
				for (std::pair<const bnf::non_terminal, std::unordered_set<bnf::symbol>>& first_set : first)
				{

					// For each non terminal A in the first sets, add the first set of A to the set
					std::vector<bnf::non_terminal> non_terminals;
					for (const bnf::symbol& element : first_set.second)
					{
						if(!element.is_terminal())
						{
							auto nt = element.get_non_terminal();

							if (nt != first_set.first)
							{
								non_terminals.push_back(nt);

								changing = true;
							}
						}
					}

					for (auto nt : non_terminals) first_set.second.erase(nt);
					for (auto nt : non_terminals) first_set.second.insert(first.at(nt).begin(), first.at(nt).end());
				}
			}
		};

		auto generate_follow_sets = [&]() {
			// Generate following sets
			bool changing = true;
			while (changing)
			{
				changing = false;

				for (const auto& rule : rules)
				{
					for (auto it = rule.rhs.begin(); it != rule.rhs.end(); it++)
					{
						if (it->is_terminal()) continue;

						auto nt = it->get_non_terminal();
						auto& first_set = first.at(nt);
						auto& following_set = follow.at(nt);

						// For each production X -> wA put FOLLOW(X) in FOLLOW(A)
						if (it == rule.rhs.end() - 1)
						{
							auto pre = following_set.size();
							following_set.insert(follow.at(rule.lhs).begin(), follow.at(rule.lhs).end());
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
							auto& next_first_set = first.at(next_nt);

							auto pre = follow.at(nt).size();
							if (auto it = std::find(next_first_set.begin(), next_first_set.end(), bnf::symbol(bnf::terminal(bnf::epsilon))); it != next_first_set.end())
							{
								// If epsilon is in FIRST(beta) then put FOLLOW(X) in FOLLOW(A)
								following_set.insert(follow.at(rule.lhs).begin(), follow.at(rule.lhs).end());
							}

							// Insert all and remove epsilon
							following_set.insert(next_first_set.begin(), next_first_set.end());
							following_set.erase(bnf::epsilon);

							if (pre != following_set.size()) changing = true;
						}
					}
				}
			}


		};

		item_sets.at(0).items.push_back(item{ rule_of(start_symbol), 0 });

		std::queue<std::size_t> set_to_do;
		expand_item_set(item_sets.at(0));
		set_to_do.push(0);

		while (!set_to_do.empty())
		{
			auto set_index = set_to_do.front();
			set_to_do.pop();

			auto size_before = item_sets.size();
			create_item_sets(item_sets.at(set_index));
			auto size_after = item_sets.size();

			for (auto i = size_before; i < size_after; i++)
				set_to_do.push(i);
		}

		// Create all first and follow sets
		create_first_follow_sets();

		// Fill first sets
		generate_first_sets();

		follow.at(start_symbol).insert(bnf::end_of_input);

		// Fill follow sets, which depend on the first sets
		generate_follow_sets();

		// For each item set
		// For each rule
		// If the bullet is at the end
		// Put a reduce in all columns in the follow set, with a rule reference, pop one off the stack
		// If the bullet is not at the end, and in front of a terminal
		// Put a shift in, with a next state, push onto the stack


		for (auto i = 0; i < item_sets.size(); i++)
		{
			for (const auto& item : item_sets.at(i).items)
			{
				// Reduce or Accept
				if (item.bullet_offset == item.rule.rhs.size())
				{
					const auto& follow_set = follow.at(item.rule.lhs);
					for (const auto& follower : follow_set)
					{
						if ((item.rule.rhs.size() == 1) && (item.rule.lhs == start_symbol))
						{
							table.insert({ { i, follower }, accept_action{} });
						}
						else
						{
							auto n = std::make_pair(std::make_pair(i, follower), reduce_action{
								static_cast<std::size_t>(std::find(rules.begin(), rules.end(), item.rule) - rules.begin())
							});
							table.insert(n);
						}
					}
				}
				// Shift or Goto
				else
				{
					for (const auto& transition : transitions)
					{
						if (transition.from == i)
						{
							if(transition.symbol.is_terminal())
								table.insert({ { i, transition.symbol }, shift_action{ transition.to } });
							else
								table.insert({ { i, transition.symbol }, goto_action{ transition.to } });
						}
					}
				}
			}
		}
	}

	void parser::parse(std::vector<bnf::terminal> input)
	{
		input.push_back(-2);
		auto it = input.begin();

		bnf::non_terminal_node root(start_symbol);

		std::stack<state> history;
		history.push(0);

		while (history.size() > 0)
		{
			auto current_state = history.top();

			auto action = table.at({ current_state, *it });

			if (std::holds_alternative<shift_action>(action))
			{
				history.push(std::get<shift_action>(action).new_state);
				it++;
			}
			else if (std::holds_alternative<reduce_action>(action))
			{
				auto rule_index = std::get<reduce_action>(action).rule_index;
				auto reduced_nt = rules.at(rule_index).lhs;

				int i = 0;
				for (; i < rules.at(rule_index).rhs.size(); i++)
				{
					history.pop();
				}


				auto next_state = table.at({ history.top(), reduced_nt });

				if (!std::holds_alternative<goto_action>(next_state))
					throw std::runtime_error("Error 1");

				history.push(std::get<goto_action>(next_state).new_state);
				std::cout << rule_index << "\n";
			}
			else if (std::holds_alternative<accept_action>(action))
			{
				std::cout << "accept\n";
				return;
			}
		}
	}
}

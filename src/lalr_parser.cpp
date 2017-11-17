#include "lalr_parser.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <assert.h>

namespace tools::lalr
{
	void parser::generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& r)
	{
		for (auto& rule : r)
		{
			this->rules.push_back(bnf::rule{ rule.first, rule.second });
			if ((rule.second.size() == 1) && (rule.second.at(0) == bnf::epsilon))
			{
				this->rules.push_back(bnf::rule{ rule.first, {} });
			}
		}
		this->rules.push_back(bnf::rule{ 0, { start_symbol } });

		item_sets.push_back(item_set());

		auto expand_item_set = [&](item_set& i) {
			for(std::size_t index = 0; index < i.items.size(); index++)
			{
				if (i.items.at(index).is_parsed())
					continue;

				if (i.items.at(index).expected_symbol().is_terminal())
					continue;

				// Bullet is not too far right and the symbol after it is a non terminal
				for (
					auto it = std::find_if(rules.begin(), rules.end(), [&](const bnf::rule& r) {
						return r.lhs == i.items.at(index).expected_symbol().get_non_terminal();
					}); 
					it != rules.end(); 
					it = std::find_if(it + 1, rules.end(), [&](const bnf::rule& r) { 
						return r.lhs == i.items.at(index).expected_symbol().get_non_terminal(); 
					}))
				{
					auto add_item = [&](item new_item) { 
						if (std::find(i.items.begin(), i.items.end(), new_item) == i.items.end())
							i.items.push_back(new_item);
					};

					auto set_item = i.items.at(index);


					bool is_empty_rule = (it->rhs.size() == 1 && (it->rhs.at(0) == bnf::epsilon));

					bnf::rule new_rule = is_empty_rule ? bnf::rule(it->lhs, {}) : *it;

					// If there are more symbols to parse after the next one
					bool first_contains_epsilon = true;
					for (int i = set_item.bullet_offset + 1; i < set_item.rule.rhs.size() && first_contains_epsilon; i++)
					{
						first_contains_epsilon = false;

						auto next_expected = set_item.rule.rhs.at(i);

						if (next_expected.is_terminal())
							add_item(item{ new_rule, 0, next_expected.get_terminal() });
						else
						{
							auto next_expected_nt = next_expected.get_non_terminal();

							for (const auto f : this->first.at(next_expected_nt))
							{
								if (f == bnf::epsilon) first_contains_epsilon = true;
								else add_item(item{ new_rule, 0, f.get_terminal() });
							}
						}
					}

					// If there could be epsilons until the end 
					if(first_contains_epsilon)
					{
						auto new_item = item{ new_rule, 0, set_item.lookahead };
						add_item(new_item);
					}
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
				if (rule.rhs.size() > 0 && !(rule.rhs.at(0) == rule.lhs))
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
						if (!element.is_terminal())
						{
							auto nt = element.get_non_terminal();

							non_terminals.push_back(nt);
						}
					}

					for (auto nt : non_terminals) first_set.second.erase(nt);
					for (auto nt : non_terminals)
					{
						if (first_set.first == nt)
							continue;

						for (const auto& new_symbol : first.at(nt))
						{
							// If the symbol is already in the set
							if (first_set.second.find(new_symbol) != first_set.second.end())
								continue;

							// If the non terminal to be added is being processed
							if (!new_symbol.is_terminal() && std::find(non_terminals.begin(), non_terminals.end(), new_symbol.get_non_terminal()) != non_terminals.end())
								continue;

							if (new_symbol.is_terminal() || (first_set.first != new_symbol.get_non_terminal()))
							{
								first_set.second.insert(new_symbol);
								changing = true;
							}
						}
					}
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

		auto first_rule = std::find_if(rules.begin(), rules.end(), [&](const auto& r) {return r.lhs == 0; });

		// Create all first and follow sets
		create_first_follow_sets();

		// Fill first sets
		generate_first_sets();

		follow.at(0).insert(bnf::end_of_input);

		// Fill follow sets, which depend on the first sets
		generate_follow_sets();
		item_sets.at(0).items.push_back(item{ *first_rule, 0, bnf::end_of_input });

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
					auto follower = item.lookahead;

					if ((item.rule.rhs.size() == 1) && (item.rule.lhs == 0))
					{
						if (table.find({ i, follower }) != table.end())
						{
							auto other = table.at({ i, follower });
							if (std::holds_alternative<shift_action>(other))
								throw conflict{ i, follower, item.rule, conflict::type::SHIFT_SHIFT };
							else
								throw conflict{ i, follower, item.rule, conflict::type::SHIFT_REDUCE };
						}
						table.insert({ { i, follower }, accept_action{} });
					}
					else
					{
						if (auto it = table.find({ i, follower }); it != table.end())
						{
							throw conflict{ i, follower, item.rule, conflict::type::SHIFT_REDUCE };
						}

						table.insert({ { i, follower }, reduce_action{
							static_cast<std::size_t>(std::distance(rules.begin(), std::find(rules.begin(), rules.end(), item.rule)))
						} });
					}
				}
				// Shift or Goto
				else
				{
					for (const auto& transition : transitions)
					{
						if (transition.from == i)
						{
							action new_action = transition.symbol.is_terminal() ?
								action(shift_action{ transition.to }) : action(goto_action{ transition.to });

							auto key = std::pair<state, bnf::symbol>(i, transition.symbol);

							action value = transition.symbol.is_terminal() ?
								action(shift_action{ transition.to }) : action(goto_action{ transition.to });

							if (auto it = table.find(key); it != table.end() && !(it->second == value))
								throw conflict{ i, transition.symbol, item.rule, conflict::type::SHIFT_SHIFT };

							table.insert({ key, value });
						}
					}
				}
			}
		}
	}

	std::unique_ptr<bnf::node> parser::parse(std::vector<bnf::terminal_node> input)
	{
		input.push_back(bnf::terminal_node(-2, ""));
		auto it = input.begin();

		bnf::non_terminal_node root(0);

		std::stack<state> history;
		std::stack<std::unique_ptr<bnf::node>> result;
		history.push(0);

		while (history.size() > 0)
		{
			auto current_state = history.top();

			auto action = ([&]() {
				if (it != input.end() && (table.find({ current_state, it->value }) != table.end()))
				{
					return table.at({ current_state, it->value });
				}
				else
				{
					auto offset = std::distance(input.begin(), it);
					throw std::runtime_error(std::string("Syntax error at ").append(std::to_string(offset)));
				}
			})();

			if (std::holds_alternative<shift_action>(action))
			{
				history.push(std::get<shift_action>(action).new_state);
				result.push(std::make_unique<bnf::node>(bnf::terminal_node(it->value, it->token)));
				it++;
			}
			else if (std::holds_alternative<reduce_action>(action))
			{
				auto reduce = std::get<reduce_action>(action);
				auto rule = rules.at(reduce.rule_index);

				std::vector<std::unique_ptr<bnf::node>> reduced_states;

				reduced_states.resize(rule.rhs.size());
				for (int i = 0; i < rule.rhs.size(); i++)
				{
					reduced_states.at(rule.rhs.size() - 1 - i) = std::move(result.top());

					result.pop();
					history.pop();
				}

				auto new_node = std::make_unique<bnf::node>(bnf::non_terminal(rule.lhs));
				std::get<bnf::non_terminal_node>(*new_node).children = std::move(reduced_states);
				result.push(std::move(new_node));

				auto next_state = table.at({ history.top(), rule.lhs });

				assert(std::holds_alternative<goto_action>(next_state));

				history.push(std::get<goto_action>(next_state).new_state);
			}
			else if (std::holds_alternative<accept_action>(action))
			{
				return std::move(result.top());
			}
		}
	}
}
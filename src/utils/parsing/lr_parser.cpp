#include "utils/parsing/lr_parser.h"

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <assert.h>
namespace utils::lr
{
	void parser::generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& r) 
	{
		this->rules = r;
		for (auto& rule : r)
		{
			if ((rule.second.size() == 1) && (rule.second.at(0) == bnf::epsilon))
			{
				this->rules.insert({ rule.first, {} });
			}
		}
		auto first_rule = this->rules.insert({ 0, { start_symbol } });

		// Create all first and follow sets
		create_first_follow_sets();

		// Fill first sets
		generate_first_sets();
		// Fill follow sets, which depend on the first sets
		follow.at(0).insert(bnf::end_of_input);
		generate_follow_sets();

		item_set first_set;
		first_set.add_item(item{ &*first_rule, 0, bnf::end_of_input }, rules, first);
		item_sets.insert(std::move(first_set));

		first_item_set = &*item_sets.begin();

		std::queue<std::reference_wrapper<const item_set>> set_to_do;
		set_to_do.push(*item_sets.begin());

		while (!set_to_do.empty())
		{
			auto set_index = set_to_do.front();
			set_to_do.pop();

			auto new_sets = create_item_sets(set_index.get());

			// Create item set
			for (auto& pair : new_sets)
			{
				auto&[sym, set] = pair;

				// Check if the item set already exists
				auto res = item_sets.insert(set);

				if(res.second)
					set_to_do.push(*res.first);

				transitions.push_back({ set_index.get(), *res.first, sym });
			}
		}


		// For each item set
		// For each rule
		// If the bullet is at the end
		// Put a reduce in all columns in the follow set, with a rule reference, pop one off the stack
		// If the bullet is not at the end, and in front of a terminal
		// Put a shift in, with a next state, push onto the stack


		for (auto& item_set : item_sets)
		{
			for (auto set_it = item_set.begin(); set_it != item_set.end(); set_it++)
			{
				const auto& item = *set_it;

				// Reduce or Accept
				if (item.bullet_offset == item.rule->second.size())
				{
					auto follower = item.lookahead;

					action value = (item.rule->second.size() == 1 && item.rule->first == 0)
						? action(accept_action())
						: action(reduce_action{ item.rule });

					auto key = std::make_pair(state(&item_set), follower);

					auto res = table.insert({ key, value });

					if (res.second)
					{
						conflict_helper.insert({ &(res.first->first), set_it });
					}
					else if (!(res.first->second == value)) // Conflict
					{
						// Build error with information about conflicting rules
						const bnf::rule& new_rule = *item.rule;
						std::size_t new_offset = item.bullet_offset;

						auto old_it = conflict_helper.at(&(table.find(key)->first));
						const bnf::rule& old_rule = *old_it->rule;
						std::size_t old_offset = old_it->bullet_offset;

						auto other = table.at({ state(&item_set), follower });
						auto conflict_type = std::holds_alternative<shift_action>(other)
							? conflict::type::SHIFT_REDUCE
							: conflict::type::REDUCE_REDUCE;

						throw conflict{
							follower,
							new_rule, new_offset,
							old_rule, old_offset,
							conflict_type
						};
					}
				}
				// Shift or Goto
				else
				{
					for (const auto& transition : transitions)
					{
						if (&transition.from == &item_set)
						{
							auto key = std::pair<state, bnf::symbol>(state(&item_set), transition.symbol);

							action value = transition.symbol.is_terminal() 
								? action(shift_action{ &transition.to }) 
								: action(goto_action{ &transition.to });

							auto res = table.insert({ key, value });

							if (res.second)
							{
								conflict_helper.insert({ &(res.first->first), set_it });
							}
							else if (!(res.first->second == value)) // Conflict
							{
								// Build error with information about conflicting rules
								const bnf::rule& new_rule = *item.rule;
								std::size_t new_offset = item.bullet_offset;

								auto old_it = conflict_helper.at(&(table.find(key)->first));
								const bnf::rule& old_rule = *old_it->rule;
								std::size_t old_offset = old_it->bullet_offset;

								throw conflict{
									transition.symbol,
									new_rule, new_offset,
									old_rule, old_offset,
									conflict::type::SHIFT_REDUCE
								};
							}
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

		std::size_t line = 1;
		std::size_t line_offset = 0;

		std::stack<state> history;
		history.push(first_item_set);

		std::stack<std::unique_ptr<bnf::node>> result;

		while (history.size() > 0)
		{
			while (it->value == bnf::new_line)
			{
				line++; it++; line_offset = 0;
			}

			auto current_state = history.top();

			auto action = ([&]() {
				if (it != input.end() && (table.find({ current_state, it->value }) != table.end()))
				{
					return table.at({ current_state, it->value });
				}
				else
				{
					throw std::runtime_error(std::string("Syntax error at line: ")
						.append(std::to_string(line))
						.append(" offset: ")
						.append(std::to_string(line_offset))
						.append(" token: ")
						.append(it->token));
				}
			})();

			if (std::holds_alternative<shift_action>(action))
			{
				history.push(std::get<shift_action>(action).new_state);
				result.push(std::make_unique<bnf::node>(bnf::terminal_node(it->value, it->token)));
				it++;
				line_offset++;
			}
			else if (std::holds_alternative<reduce_action>(action))
			{
				auto reduce = std::get<reduce_action>(action);

				std::vector<std::unique_ptr<bnf::node>> reduced_states;

				reduced_states.resize(reduce.rule->second.size());
				for (int i = 0; i < reduce.rule->second.size(); i++)
				{
					reduced_states.at(reduce.rule->second.size() - 1 - i) = std::move(result.top());

					result.pop();
					history.pop();
				}

				auto new_node = std::make_unique<bnf::node>(bnf::non_terminal_node(reduce.rule->first,
					std::move(reduced_states)));
				result.push(std::move(new_node));

				auto next_state = table.at({ history.top(), reduce.rule->first });

				assert(std::holds_alternative<goto_action>(next_state));

				history.push(std::get<goto_action>(next_state).new_state);
			}
			else if (std::holds_alternative<accept_action>(action))
			{
				return std::move(result.top());
			}
		}
	}

	std::unordered_map<bnf::symbol, item_set> parser::create_item_sets(const item_set & i)
	{
		std::unordered_map<bnf::symbol, item_set> new_sets;
		for (const auto& item : i)
		{
			// Filter out rules that have no expected symbols left
			if (item.bullet_offset >= item.rule->second.size())
				continue;

			// Get the expected symbol of the current item
			auto sym = item.expected_symbol();

			// And create a new item set if it doesn't exist yet in this iteration
			auto it = new_sets.find(sym);
			if (it == new_sets.end())
				it = new_sets.insert({ sym, item_set() }).first;

			// Create new item with dot shifted right
			lr::item next_item(item.rule, item.bullet_offset + 1, item.lookahead);

			// Insert new item in the item set
			it->second.add_item(std::move(next_item), rules, first);
		}
		return new_sets;
	}

	void parser::create_first_follow_sets()
	{
		for (const auto& rule : rules)
		{
			// If first set doesnt exist yet make one
			if (auto loc = first.find(rule.first); loc == first.end())
				first.insert({ rule.first, std::unordered_set<bnf::symbol>() });
			if (rule.second.size() > 0 && !(rule.second.at(0) == rule.first))
				first.at(rule.first).insert(rule.second.at(0).value);

			// If following set doesnt exist yet make one
			if (auto loc = follow.find(rule.first); loc == follow.end())
				follow.insert({ rule.first, std::unordered_set<bnf::symbol>() });
		}
	}

	void parser::generate_first_sets()
	{
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
	}

	void parser::generate_follow_sets()
	{
		// Generate following sets
		bool changing = true;
		while (changing)
		{
			changing = false;

			for (const auto& rule : rules)
			{
				for (auto it = rule.second.begin(); it != rule.second.end(); it++)
				{
					if (it->is_terminal()) continue;

					auto nt = it->get_non_terminal();
					auto& first_set = first.at(nt);
					auto& following_set = follow.at(nt);

					// For each production X -> wA put FOLLOW(X) in FOLLOW(A)
					if (it == rule.second.end() - 1)
					{
						auto pre = following_set.size();
						following_set.insert(follow.at(rule.first).begin(), follow.at(rule.first).end());
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
							following_set.insert(follow.at(rule.first).begin(), follow.at(rule.first).end());
						}

						// Insert all and remove epsilon
						following_set.insert(next_first_set.begin(), next_first_set.end());
						following_set.erase(bnf::epsilon);

						if (pre != following_set.size()) changing = true;
					}
				}
			}
		}


	}
}


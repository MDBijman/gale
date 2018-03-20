#pragma once
#include "bnf_grammar.h"
#include "parser.h"

#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <assert.h>
#include <vector>

namespace utils::lr
{
	using ruleset = std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>;

	/*
	* The first set of a nonterminal N contains all terminals that are first on
	* the rules with N on the lhs, including epsilon if there could be no tokens consumed.
	*/
	using first_set = std::unordered_map<bnf::non_terminal, std::unordered_set<bnf::symbol>>;

	/*
	* The follow set of a nonterminal N contains all terminals that could be the first token
	* consumed after the rhs of N is fully consumed.
	*/
	using follow_set = std::unordered_map<bnf::non_terminal, std::unordered_set<bnf::symbol>>;

	/*
	* An item is a combination of a rule an offset, and a lookahead
	* The offset within the rule indicates the progress of parsing.
	* The lookahead contains the symbol that must follow this item.
	* There are often many different lookaheads possible, thus many
	* copies of an item within an item set differing only by their lookahead.
	*
	* TODO find efficient way of merging these items
	*/
	struct item
	{
		item(ruleset::const_pointer r, std::size_t off, bnf::terminal look) :
			rule(r), bullet_offset(off), lookahead(look) {}

		bool is_parsed() const
		{
			return bullet_offset >= rule->second.size();
		}

		bnf::symbol expected_symbol() const
		{
			return rule->second.at(bullet_offset);
		}

		bool operator==(const item& other) const
		{
			return (bullet_offset == other.bullet_offset) 
				&& (lookahead == other.lookahead) 
				&& (rule == other.rule);
		}

		bool operator<(const item& o) const
		{
			return std::tie(bullet_offset, lookahead, rule) < std::tie(o.bullet_offset, o.lookahead, o.rule);
		}

		const ruleset::const_pointer rule;
		const std::size_t bullet_offset;
		const bnf::terminal lookahead;
	};
}

namespace std
{
	template<>
	struct hash<utils::lr::item>
	{
		size_t operator()(const utils::lr::item& i) const
		{
			return hash<utils::lr::ruleset::const_pointer>()(i.rule)
				^ (hash<std::size_t>()(i.bullet_offset) << 1)
				^ (hash<utils::bnf::terminal>()(i.lookahead) << 2);
		}
	};
}

namespace utils::lr
{
	/*
	* An item set contains the set of items that could correspond to the input when the parser is in this state.
	*/
	struct item_set
	{
	private:
		std::unordered_set<item> items;

	public:
		bool operator==(const item_set& other) const
		{
			return items == other.items;
		}

		bool contains_item(const item& item) const
		{
			return items.find(item) != items.end();
		}

		void add_item(item&& item, const ruleset& rules, const first_set& first)
		{
			if (!contains_item(item))
			{
				auto pos = items.insert(std::move(item));
				add_derivatives(*pos.first, rules, first);
			}
		}

		using const_iterator = decltype(items)::const_iterator;

		const_iterator begin() const
		{
			return items.begin();
		}

		const_iterator end() const
		{
			return items.end();
		}

		std::size_t size() const
		{
			return items.size();
		}

	private:
		void add_derivatives(const item& item, const ruleset& rules, const first_set& first)
		{
			if (item.is_parsed()) return;
			if (item.expected_symbol().is_terminal()) return;

			// For all rules that begin with the expected non terminal
			auto range = rules.equal_range(item.expected_symbol().get_non_terminal());
			for (auto it = range.first; it != range.second; ++it)
			{
				ruleset::const_pointer new_rule = &*it;

				// If the rule has only an epsilon on the rhs, there is also one with an empty rhs
				bool is_empty_rule = (it->second.size() == 1 && (it->second.at(0) == bnf::epsilon));
				if (is_empty_rule)
				{
					auto range = rules.equal_range(it->first);
					new_rule = &*std::find_if(range.first, range.second, [](auto& x) {
						return x.second.size() == 0;
					});
				}

				// If there are more symbols to parse after the next one
				bool successive_epsilons = true;
				for (std::size_t j = item.bullet_offset + 1; j < item.rule->second.size() && successive_epsilons; j++)
				{
					successive_epsilons = false;

					const auto& next_expected = item.rule->second.at(j);

					if (next_expected.is_terminal())
					{
						add_item(lr::item{ new_rule, 0, next_expected.get_terminal() }, rules, first);
					}
					else
					{
						auto next_expected_nt = next_expected.get_non_terminal();

						for (const auto& f : first.at(next_expected_nt))
						{
							assert(f.is_terminal());
							if (f == bnf::epsilon) successive_epsilons = true;
							else
							{
								add_item(lr::item{ new_rule, 0, f.get_terminal() }, rules, first);
							}
						}
					}
				}

				// If there could be epsilons until the end 
				if (successive_epsilons)
				{
					add_item(lr::item{ new_rule, 0, item.lookahead }, rules, first);
				}
			}
		}
	};

	using state = const item_set*;

	/*
	* A goto action is performed after a reduce to move to the right state to resume parsing.
	*/
	struct goto_action
	{
		state new_state;
	};

	static bool operator==(const goto_action& a, const goto_action& b)
	{
		return a.new_state == b.new_state;
	}

	/*
	* A reduce action is performed after the entire RHS of a rule is parsed. The rule_index is
	* the index of the reduced rule in the vector of rules.
	*/
	struct reduce_action
	{
		const ruleset::const_pointer rule;
	};

	static bool operator==(const reduce_action& a, const reduce_action& b)
	{
		return a.rule == b.rule;
	}

	/*
	* A shift action is performed to shift a symbol from the input onto the stack.
	*/
	struct shift_action
	{
		state new_state;
	};

	static bool operator==(const shift_action& a, const shift_action& b)
	{
		return a.new_state == b.new_state;
	}

	/*
	* An accept action is performed when parsing is finished and indicates a syntactically correct input.
	*/
	struct accept_action
	{
	};

	static bool operator==(const accept_action& a, const accept_action& b)
	{
		return true;
	}

	/*
	* A concflict is thrown when during the generation of the parse table two different actions end up
	* in the same position. This indicates a conflict in the given ruleset.
	*/
	struct conflict
	{
		bnf::symbol expected;

		bnf::rule rule;
		std::size_t offset;

		bnf::rule other;
		std::size_t other_offset;

		enum class type {
			REDUCE_REDUCE, SHIFT_REDUCE
		} type;
	};

	/*
	* An action can be either a goto, reduce, shift, or an accept.
	*/
	using action = std::variant<goto_action, reduce_action, accept_action, shift_action>;

}

namespace std
{
	template<>
	struct std::hash<std::pair<utils::lr::state, utils::bnf::symbol>>
	{
		size_t operator()(const pair<utils::lr::state, utils::bnf::symbol>& key) const
		{
			return (hash<utils::lr::state>()(key.first) << 1) ^ (hash<utils::bnf::symbol>()(key.second));
		}
	};

	template<>
	struct hash<utils::lr::item_set>
	{
		size_t operator()(const utils::lr::item_set& i) const
		{
			std::size_t h = 0;
			for (const auto& item : i)
			{
				h ^= hash<utils::lr::item>()(item);
			}
			return h;
		}
	};
}

namespace utils::lr
{

	/*
	* The parse table maps a state and a symbol to an action for the parser. The action can be
	* a goto, a reduce, a shift, or an accept.
	*/
	using parse_table = std::unordered_map<std::pair<state, bnf::symbol>, action>;

	/*
	* An item set transition describes as the name suggests a possible transition
	* from one item set to another. When the parser is in the
	* item set at index 'from' and reads the 'symbol', it moves to the item set at index 'to'.
	*/
	struct item_set_transition
	{
		const item_set& from;
		const item_set& to;
		bnf::symbol symbol;
	};

	class parser : public utils::parser
	{
	public:
		void generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& rules) override;

		std::unique_ptr<bnf::node> parse(std::vector<bnf::terminal_node> input) override;

		ruleset rules;
		std::unordered_set<item_set> item_sets;
		std::vector<item_set_transition> transitions;
		first_set first;
		follow_set follow;

		const item_set* first_item_set;
		parse_table table;
		std::unordered_map<const parse_table::key_type*, item_set::const_iterator> conflict_helper;

	private:
		std::unordered_map<bnf::symbol, item_set> create_item_sets(const item_set& i);
		void create_first_follow_sets();
		void generate_first_sets();
		void generate_follow_sets();
	};
}
//
//namespace std
//{
//	template<> struct hash<utils::lr::parse_table::const_iterator>
//	{
//		std::size_t operator()(const utils::lr::parse_table::const_iterator& it) const
//		{
//		}
//	};
//}

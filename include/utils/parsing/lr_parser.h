#pragma once
#include "bnf_grammar.h"
#include "parser.h"

#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tools::lr
{
	/*
	* An item set transition describes as the name suggests a possible transition
	* from one item set to another. When the parser is in the
	* item set at index 'from' and reads the 'symbol', it moves to the item set at index 'to'.
	*/
	struct item_set_transition
	{
		const std::size_t from;
		const std::size_t to;
		bnf::symbol symbol;
	};

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
		bool is_parsed() const
		{
			return bullet_offset >= rule.rhs.size();
		}

		bnf::symbol expected_symbol() const
		{
			return rule.rhs.at(bullet_offset);
		}

		bool operator==(const item& other) const
		{
			return (rule == other.rule) && (bullet_offset == other.bullet_offset) && (lookahead == other.lookahead);
		}

		const bnf::rule rule;
		std::size_t bullet_offset;
		bnf::terminal lookahead;
	};

	/*
	* An item set contains the set of items that could correspond to the input when the parser is in this state.
	*/
	struct item_set
	{
	public:
		bool operator==(const item_set& other) const
		{
			return items == other.items;
		}

		std::vector<item> items;
	};

	using state = std::size_t;

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
		std::size_t rule_index;
	};
	
	static bool operator==(const reduce_action& a, const reduce_action& b)
	{
		return a.rule_index == b.rule_index;
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
		int item_set;
		bnf::symbol expected;
		bnf::rule rule;

		enum class type {
			SHIFT_SHIFT, SHIFT_REDUCE
		} type;
	};

	/*
	* An action can be either a goto, reduce, shift, or an accept.
	*/
	using action = std::variant<goto_action, reduce_action, accept_action, shift_action>;

	namespace detail
	{
		/*
		* Implements a hash for a pair of a state and a symbol. The hash is used in the parse table.
		*/
		struct parse_table_hasher
		{
			std::size_t operator()(const std::pair<state, bnf::symbol>& key) const
			{
				return (std::hash<state>()(key.first) << 1) ^ (std::hash<bnf::symbol>()(key.second));
			}
		};
	}

	/*
	* The parse table maps a state and a symbol to an action for the parser. The action can be
	* a goto, a reduce, a shift, or an accept.
	*/
	using parse_table = std::unordered_map<std::pair<state, bnf::symbol>, action, detail::parse_table_hasher>;

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

	class parser : public tools::parser
	{
	public:
		void generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& rules) override;

		std::unique_ptr<bnf::node> parse(std::vector<bnf::terminal_node> input) override;

		std::vector<bnf::rule> rules;
		std::vector<item_set> item_sets;
		std::vector<item_set_transition> transitions;
		first_set first;
		follow_set follow;

		parse_table table;
	};
}

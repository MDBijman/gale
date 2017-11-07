#pragma once
#include "bnf_grammar.h"
#include "parser.h"
#include <variant>
#include <unordered_map>
#include <unordered_set>

namespace tools::lalr
{
	struct item_set_transition
	{
		const std::size_t from;
		const std::size_t to;
		bnf::symbol symbol;
	};

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

	struct goto_action
	{
		state new_state;
	};
	
	static bool operator==(const goto_action& a, const goto_action& b)
	{
		return a.new_state == b.new_state;
	}

	struct reduce_action
	{
		std::size_t rule_index;
	};
	
	static bool operator==(const reduce_action& a, const reduce_action& b)
	{
		return a.rule_index == b.rule_index;
	}

	struct shift_action
	{
		state new_state;
	};
	
	static bool operator==(const shift_action& a, const shift_action& b)
	{
		return a.new_state == b.new_state;
	}

	struct accept_action
	{
	};
	
	static bool operator==(const accept_action& a, const accept_action& b)
	{
		return true;
	}

	/*
	* An action can be either a goto, reduce, or an accept.
	*/
	using action = std::variant<goto_action, reduce_action, accept_action, shift_action>;

	namespace detail
	{
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

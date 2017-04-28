#pragma once
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <string>
#include <variant>
#include <algorithm>
#include <iostream>
#include <set>
#include "ast.h"
#include "lexer.h"

namespace bnf_parsing
{
	using terminal = lexer::token_id;
	constexpr terminal epsilon = -1;
	constexpr terminal end_of_input = -2;

	/*
	* EBNF non terminal type.
	*/
	using non_terminal = uint64_t;

	struct symbol
	{
		symbol(std::variant<terminal, non_terminal> symbol) : content(symbol) {}
		symbol(terminal t) : content(t) {}
		symbol(non_terminal nt) : content(nt) {}

		bool is_terminal() const
		{
			return std::holds_alternative<terminal>(content);
		}

		terminal get_terminal() const
		{
			return std::get<terminal>(content);
		}

		non_terminal get_non_terminal() const
		{
			return std::get<non_terminal>(content);
		}

		bool matches(symbol other, const std::multimap<non_terminal, std::vector<symbol>>& mapping) const
		{
			if (other.is_terminal() && is_terminal() && (other.get_terminal() == get_terminal())) return true;
			if (!other.is_terminal() && !is_terminal() && (other.get_non_terminal() == get_non_terminal())) true;
			if (is_terminal() && (get_terminal() == epsilon)) return true;

			if (other.is_terminal() && !is_terminal())
			{
				auto other_symbol = other.get_terminal();
				auto this_non_terminal = get_non_terminal();

				auto possible_matches = mapping.equal_range(this_non_terminal);

				for (auto it = possible_matches.first; it != possible_matches.second; ++it)
				{
					if (it->second.at(0).matches(other_symbol, mapping))
					{
						return true;
					}
				}
			}

			return false;
		}

	private:
		std::variant<terminal, non_terminal> content;
	};

	using symbol_definitions = std::vector<std::pair<std::string, std::vector<std::string>>>;

	class rules
	{
	public:

		rules(const lexer::token_definitions& td, const symbol_definitions& sd) : tokens(td), symbols(sd) 
		{
			terminal t_generator = 0;
			for (decltype(auto) token_definition : tokens)
			{
				terminals.insert({ token_definition.first, t_generator });
				t_generator++;
			}
			terminals.insert({ "end_of_input", end_of_input });
			terminals.insert({ "epsilon", epsilon });

			non_terminal nt_generator = 0;
			for (decltype(auto) symbol_definition : symbols)
			{
				non_terminals.insert({ symbol_definition.first, nt_generator });
				nt_generator++;
			}

			for (decltype(auto) symbol_definition : symbols)
			{
				auto& name = symbol_definition.first;
				auto& rhs = symbol_definition.second;

				auto new_rule = mapping.insert({ non_terminals.at(name), std::vector<symbol>() });

				for (decltype(auto) rule_segment : rhs)
				{
					if (terminals.find(rule_segment) != terminals.end())
					{
						new_rule->second.push_back(terminals.at(rule_segment));
					}
					else if (non_terminals.find(rule_segment) != non_terminals.end())
					{
						new_rule->second.push_back(non_terminals.at(rule_segment));
					}
					else
					{
						throw std::runtime_error("Unknown identifier");
					}
				}
			}
		}

		symbol to_symbol(const std::string& symbol_name) const
		{
			if (terminals.find(symbol_name) != terminals.end())
				return terminals.at(symbol_name);
			else if (non_terminals.find(symbol_name) != non_terminals.end())
				return non_terminals.at(symbol_name);
			else
				throw std::runtime_error("Unknown symbol");
		}

		std::string to_string(symbol symbol) const
		{
			if (symbol.is_terminal())
			{
				for (auto pair : terminals)
				{
					if (pair.second == symbol.get_terminal())
						return pair.first;
				}
			}
			else
			{
				for (auto pair : non_terminals)
				{
					if (pair.second == symbol.get_non_terminal())
						return pair.first;
				}
			}
			throw std::runtime_error("Unknown symbol");
		}

		std::multimap<non_terminal, std::vector<symbol>> mapping;

		std::unordered_map<std::string, terminal> terminals;
		std::unordered_map<std::string, non_terminal> non_terminals;

		const lexer::token_definitions tokens;
		const symbol_definitions symbols;
	};

	class parser
	{
	public:
		parser(const rules& rules) : rules(rules) {}

		ast::node<symbol>* parse(const std::string& initial, std::vector<terminal> input) const;

	private:
		void parse(std::stack<ast::node<symbol>*>& stack, std::vector<terminal>& input) const;
		void prune(ast::node<symbol>* tree) const;

		const rules rules;
	};
}
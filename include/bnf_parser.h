#pragma once
#include "lexer.h"
#include "ast.h"
#include <stack>

namespace language
{
	namespace bnf
	{
		using terminal = lexing::token_id;
		constexpr terminal epsilon = -1;
		constexpr terminal end_of_input = -2;

		using non_terminal = uint64_t;

		/*
		* \brief A symbol contains either a terminal or a non terminal. Used for checking rule matching.
		*/
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

			bool operator== (const symbol& other)
			{
				if (is_terminal() != other.is_terminal()) return false;
				if (is_terminal())
					return get_terminal() == other.get_terminal();
				else
					return get_non_terminal() == other.get_non_terminal();
			}

			std::variant<terminal, non_terminal> content;
		};

		struct rule
		{
			rule(const non_terminal& lhs, const std::vector<symbol>& rhs) : lhs(lhs), rhs(rhs) {}

			const non_terminal lhs;
			const std::vector<symbol> rhs;
		};

		enum class error_code
		{
			TERMINAL_MISMATCH,
			NO_MATCHING_RULE,
			UNEXPECTED_END_OF_INPUT,
			UNEXPECTED_NON_TERMINAL
		};

		struct error
		{
			error_code type;
			std::string message;
		};

		class rules
		{
		public:
			rules(std::multimap<non_terminal, std::vector<symbol>> rules) : bnf_rules(rules) {}

			bool has_rule(non_terminal symbol) const
			{
				return bnf_rules.find(symbol) != bnf_rules.end();
			}

			auto get_rules(non_terminal symbol) const
			{
				return bnf_rules.equal_range(symbol);
			}

			std::variant<const std::vector<symbol>*, error> match(non_terminal symbol, terminal input_token) const
			{
				auto rules = get_rules(symbol);

				// Find a rule that matches
				auto rule_location = std::find_if(rules.first, rules.second, [&](auto& rule) {
					return rule.second.at(0).matches(input_token, bnf_rules);
				});
				if (rule_location == rules.second)
					return error{ error_code::NO_MATCHING_RULE, std::to_string((int)input_token) };

				return &rule_location->second;
			}

		private:
			std::multimap<non_terminal, std::vector<symbol>> bnf_rules;

			struct table_hash
			{
				std::size_t operator()(std::pair<non_terminal, terminal> const& s) const
				{
					std::size_t h1 = std::hash<non_terminal>{}(s.first);
					std::size_t h2 = std::hash<terminal>{}(s.second);
					return h1 ^ (h2 << 1);
				}
			};
			std::unordered_map<std::pair<non_terminal, terminal>, std::vector<symbol>, table_hash> table;
		};

		class parser
		{
		public:
			parser(rules rules) : rules(std::move(rules)) {}
			parser(parser&& other) : rules(std::move(other.rules)) {}

			void operator= (parser&& other)
			{
				rules = std::move(other.rules);
			}

			std::variant<ast::node<symbol>*, error> parse(non_terminal begin_symbol, std::vector<terminal> input) const;

		private:
			rules rules;
		};
	}

}

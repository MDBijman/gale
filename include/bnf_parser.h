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

		/*
		* \brief A symbol contains either a terminal or a non terminal. Used for checking rule matching.
		*/
		struct symbol
		{
			symbol(std::variant<terminal, non_terminal> symbol) : value(symbol) {}
			symbol(terminal t) : value(t) {}
			symbol(non_terminal nt) : value(nt) {}

			bool is_terminal() const
			{
				return std::holds_alternative<terminal>(value);
			}

			terminal get_terminal() const
			{
				return std::get<terminal>(value);
			}

			non_terminal get_non_terminal() const
			{
				return std::get<non_terminal>(value);
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

			std::variant<terminal, non_terminal> value;
		};

		struct node
		{
			node(symbol s) : value(s) {}
			virtual ~node() 
			{
				for (auto child : children)
					delete child;
			}

			std::vector<node*> children;
			symbol value;
		};

		struct rule
		{
			rule(const non_terminal& lhs, const std::vector<symbol>& rhs) : lhs(lhs), rhs(rhs) {}

			const non_terminal lhs;
			const std::vector<symbol> rhs;
		};

		class parser
		{
		public:
			parser() {}
			parser(parser&& other) : rules(std::move(other.rules)) {}

			void operator= (parser&& other)
			{
				rules = std::move(other.rules);
			}

			std::variant<node*, error> parse(non_terminal begin_symbol, std::vector<terminal> input) const;

			std::variant<const std::vector<symbol>*, error> match(non_terminal symbol, terminal input_token) const
			{
				// All rules with the right non terminal on the lhs
				auto possible_matches = rules.equal_range(symbol);

				if (possible_matches.first == possible_matches.second)
					return error{ error_code::NO_MATCHING_RULE, std::to_string((int)input_token) };

				// Find a rule that matches
				auto rule_location = std::find_if(possible_matches.first, possible_matches.second, [&](auto& rule) {
					return rule.second.at(0).matches(input_token, rules);
				});

				if (rule_location == possible_matches.second)
					return error{ error_code::NO_MATCHING_RULE, std::to_string((int)input_token) };

				return &rule_location->second;
			}

			parser& add_rule(const rule& r)
			{
				rules.insert({ r.lhs, r.rhs });
				return *this;
			}

			terminal new_terminal()
			{
				return t_generator++;
			}

			non_terminal new_non_terminal()
			{
				return nt_generator++;
			}

		private:
			std::multimap<non_terminal, std::vector<symbol>> rules;

			terminal t_generator = 1;
			non_terminal nt_generator = 1;
		};
	}
}

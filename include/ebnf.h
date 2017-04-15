#pragma once
#include <unordered_map>
#include <variant>

namespace ebnf
{
	enum class terminal
	{
		NAME,
		ASSIGNMENT,
		STRING,
		IDENTIFIER,
		END_OF_RULE,
		ZERO_OR_MORE,
		ONE_OR_MORE,
		ZERO_OR_ONE,
		BEGIN_GROUP,
		END_GROUP,
		ALTERNATION_SIGN,
		EXCEPTION,
		EPSILON,
		END_OF_INPUT,
	};

	enum class non_terminal
	{
		RULESET,
		RULE,
		TERMINAL,

		PRIMARY,
		RHS_ALTERNATION,
		RHS_EXCEPTION,
		OPTIONAL_ALTERNATION,
		OPTIONAL_EXCEPTION,
		ZERO_OR_MORE_ALTERNATION,
		OPTIONAL_MULTIPLIER,
		COMBINATION,

		GROUP,
		EXCEPTION,
		CONCATENTATION,
	};

	struct symbol
	{
		symbol(std::variant<terminal, non_terminal> symbol) : content(symbol) {}
		symbol(terminal t) : content(t) {}
		symbol(non_terminal nt) : content(nt) {}

		bool is_terminal()
		{
			return std::holds_alternative<terminal>(content);
		}

		terminal get_terminal()
		{
			return std::get<terminal>(content);
		}

		non_terminal get_non_terminal()
		{
			return std::get<non_terminal>(content);
		}

		bool matches(symbol other, std::unordered_map<non_terminal, std::vector<std::vector<symbol>>>& mapping)
		{
			if (other.is_terminal() && is_terminal() && (other.get_terminal() == get_terminal())) return true;
			if (!other.is_terminal() && !is_terminal() && (other.get_non_terminal() == get_non_terminal())) true;
			if (is_terminal() && (get_terminal() == terminal::EPSILON)) return true;

			if (other.is_terminal() && !is_terminal())
			{
				auto other_symbol = other.get_terminal();
				auto this_non_terminal = get_non_terminal();

				auto possible_matches = mapping.at(this_non_terminal);

				for (auto possible_match : possible_matches)
				{
					if (possible_match.at(0).matches(other_symbol, mapping))
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

	using rules = std::unordered_map<non_terminal, std::vector<std::vector<symbol>>>;
}
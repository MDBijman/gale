#pragma once
#include "utils/lexing/lexer.h"

#include <memory>
#include <vector>
#include <variant>

namespace tools::bnf
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

		bool matches(const symbol& other, const std::multimap<non_terminal, std::vector<symbol>>& mapping) const
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

		bool operator==(const symbol& other) const
		{
			if (is_terminal() != other.is_terminal()) return false;
			if (is_terminal())
				return get_terminal() == other.get_terminal();
			else
				return get_non_terminal() == other.get_non_terminal();
		}

		friend std::ostream& operator<< (std::ostream& stream, const symbol& sym)
		{
			if(std::holds_alternative<terminal>(sym.value))
				stream << std::get<terminal>(sym.value);
			else if(std::holds_alternative<non_terminal>(sym.value))
				stream << std::get<non_terminal>(sym.value);
			return stream;
		}

		operator std::string() const
		{ 
			if(std::holds_alternative<terminal>(value))
				return std::to_string(std::get<terminal>(value));
			else if(std::holds_alternative<non_terminal>(value))
				return std::to_string(std::get<non_terminal>(value));
		}

		std::variant<terminal, non_terminal> value;
	};

	struct terminal_node
	{
		terminal_node(terminal s, const std::string& t) : value(s), token(t) {}
		terminal value;
		std::string token;
	};

	struct non_terminal_node
	{
		non_terminal_node(non_terminal s) : value(s) {}
		non_terminal value;
		std::vector<std::unique_ptr<std::variant<terminal_node, non_terminal_node>>> children;
	};

	using node = std::variant<terminal_node, non_terminal_node>;

	struct rule
	{
		rule(const non_terminal& lhs, const std::vector<symbol>& rhs) : lhs(lhs), rhs(rhs) {}

		bool operator==(const rule& other) const
		{
			return (lhs == other.lhs) && (rhs == other.rhs);
		}

		non_terminal lhs;
		std::vector<symbol> rhs;
	};
}

namespace std
{
	template<> struct std::hash<tools::bnf::symbol>
	{
		std::size_t operator()(const tools::bnf::symbol& s) const
		{
			return std::hash<decltype(s.value)>()(s.value);
		}
	};
}


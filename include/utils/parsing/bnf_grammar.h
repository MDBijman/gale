#pragma once
#include "utils/lexing/lexer.h"

#include <memory>
#include <vector>
#include <variant>

namespace utils::bnf
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
			return value == other.value;
		}

		bool operator<(const symbol& other) const
		{
			return value < other.value;
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
	using rule = std::pair<non_terminal, std::vector<symbol>>;

	bool operator==(const rule& r1, const rule& r2);
}

namespace std
{
	template<> struct std::hash<utils::bnf::symbol>
	{
		size_t operator()(const utils::bnf::symbol& s) const
		{
			return hash<decltype(s.value)>()(s.value);
		}
	};

	template<> struct std::hash<std::vector<utils::bnf::symbol>>
	{
		size_t operator()(const std::vector<utils::bnf::symbol>& s) const
		{
			size_t h = hash<std::size_t>()(s.size());

			if (s.size() > 0)
				h ^= hash<utils::bnf::symbol>()(s.at(0));
			if (s.size() > 1)
				h ^= hash<utils::bnf::symbol>()(s.at(1));
			if (s.size() > 2)
				h ^= hash<utils::bnf::symbol>()(s.at(2));
			return h;
		}
	};

	template<> struct std::hash<utils::bnf::rule>
	{
		size_t operator()(const utils::bnf::rule& s) const
		{
			return hash<utils::bnf::non_terminal>()(s.first)
				^ (hash<std::vector<utils::bnf::symbol>>()(s.second) << 1);
		}
	};
}


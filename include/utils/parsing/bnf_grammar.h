#pragma once
#include "utils/lexing/lexer.h"
#include "utils/memory/data_store.h"

#include <memory>
#include <vector>
#include <variant>
#include <stack>

namespace utils::bnf
{
	using terminal = lexing::token_id;
	constexpr terminal epsilon = -1;
	constexpr terminal end_of_input = -2;
	constexpr terminal new_line = -3;

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
				return std::to_string(std::get<terminal>(value))+"t";
			else if(std::holds_alternative<non_terminal>(value))
				return std::to_string(std::get<non_terminal>(value))+"nt";
			else throw std::runtime_error("Symbol holds invalid value");
		}

		std::variant<terminal, non_terminal> value;
	};

	using rule = std::pair<non_terminal, std::vector<symbol>>;
	bool operator==(const rule& r1, const rule& r2);

	enum class node_type
	{
		TERMINAL, NON_TERMINAL
	};

	struct node
	{
		node_type kind;
		size_t value_id;
	};

	using node_id = size_t;

	using terminal_node = std::pair<terminal, std::string>;
	using non_terminal_node = std::pair<non_terminal, std::vector<size_t>>;

	struct tree
	{
		using index = size_t;

		memory::dynamic_store<node> nodes;
		memory::dynamic_store<terminal_node> terminals;
		memory::dynamic_store<non_terminal_node> non_terminals;
		node_id root_id;

		bool is_leaf(index i)
		{
			return nodes.get_at(i).kind == node_type::TERMINAL;
		}

		index root()
		{
			return root_id;
		}

		size_t size()
		{
			return nodes.get_data().size();
		}

		std::vector<index>& get_children_of(index i)
		{
			return non_terminals.get_at(nodes.get_at(i).value_id).second;
		}

		void set_root(node_id id)
		{
			root_id = id;
		}

		size_t create_terminal(terminal_node v)
		{
			auto new_node = nodes.create();
			nodes.get_at(new_node).kind = node_type::TERMINAL;
			auto t_id = terminals.create();
			terminals.get_at(t_id) = v;
			nodes.get_at(new_node).value_id = t_id;
			return new_node;
		}

		size_t create_non_terminal(non_terminal_node v)
		{
			auto new_node = nodes.create();
			nodes.get_at(new_node).kind = node_type::NON_TERMINAL;
			auto t_id = non_terminals.create();
			non_terminals.get_at(t_id) = v;
			nodes.get_at(new_node).value_id = t_id;
			return new_node;
		}

		node& get_node(size_t i)
		{
			return nodes.get_at(i);
		}

		terminal_node& get_terminal(size_t i)
		{
			return terminals.get_at(i);
		}

		non_terminal_node& get_non_terminal(size_t i)
		{
			return non_terminals.get_at(i);
		}

		void free(size_t i)
		{
			auto& node = nodes.get_at(i);
			if (node.kind == node_type::TERMINAL)
				terminals.free_at(node.value_id);
			else
				non_terminals.free_at(node.value_id);
			nodes.free_at(i);
		}
	};
}

namespace std
{
	template<> struct std::hash<utils::bnf::symbol>
	{
		size_t operator()(const utils::bnf::symbol& s) const
		{
			return s.is_terminal() ?
				s.get_terminal() :
				~s.get_non_terminal();
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


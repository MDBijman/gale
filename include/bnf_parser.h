#pragma once
#include "lexer.h"
#include "bnf_grammar.h"
#include <stack>
#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace tools::bnf
{
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

	class parser
	{
	public:
		parser() {}

		parser(parser&& other) : rules(std::move(other.rules)), table(std::move(other.table)) {}
		parser& operator=(parser&& other)
		{
			rules = std::move(other.rules);
			table = std::move(other.table);
			return *this;
		}

		std::variant<const std::vector<symbol>*, error> match(non_terminal lhs, terminal input_token) const;

		/*
		* \brief Returns a parse tree (concrete syntax tree) describing the input.
		*/
		std::variant<std::unique_ptr<node>, error> parse(non_terminal begin_symbol, std::vector<terminal_node> input);

		parser& new_rule(const rule& r)
		{
			rules.insert({ r.lhs, r.rhs });
			table_is_old = true;
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
		void generate_table();

		std::multimap<non_terminal, std::vector<symbol>> rules;

		struct table_hash
		{
			std::size_t operator()(const std::pair<non_terminal, terminal>& p) const
			{
				return (std::hash<non_terminal>()(p.first) << 1) ^ (std::hash<terminal>()(p.second));
			}
		};
		std::unordered_map<std::pair<non_terminal, terminal>, decltype(rules.begin()), table_hash> table;
		bool table_is_old = true;

		terminal t_generator = 1;
		non_terminal nt_generator = 1;
	};
}

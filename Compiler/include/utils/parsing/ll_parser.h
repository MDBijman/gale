#pragma once
#include "utils/lexing/lexer.h"
#include "bnf_grammar.h"
#include "parser.h"

#include <stack>
#include <memory>
#include <unordered_set>
#include <unordered_map>

namespace utils::ll
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

	class parser : public utils::parser
	{
	public:
		parser() {}

		parser(parser&& other) :  table(std::move(other.table)) {}
		parser& operator=(parser&& other)
		{
			table = std::move(other.table);
			return *this;
		}

		void generate(bnf::non_terminal start_symbol, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>& rules) override;

		bnf::tree parse(std::vector<bnf::terminal_node> input) override;

	private:
		bnf::non_terminal start_symbol;

		struct table_hash
		{
			std::size_t operator()(const std::pair<bnf::non_terminal, bnf::terminal>& p) const
			{
				return (std::hash<bnf::non_terminal>()(p.first) << 1) ^ (std::hash<bnf::terminal>()(p.second));
			}
		};
		std::unordered_map<std::pair<bnf::non_terminal, bnf::terminal>, std::multimap<bnf::non_terminal, std::vector<bnf::symbol>>::iterator, table_hash> table;
	};
}

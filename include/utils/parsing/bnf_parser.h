#pragma once
#include "bnf_grammar.h"
#include "parser.h"
#include "lr_parser.h"

#include <string>
#include <map>
#include <variant>
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
		parser() : implementation(new lalr::parser()) {}

		parser(parser&& other) : rules(std::move(other.rules)), implementation(std::move(other.implementation)) {}
		parser& operator=(parser&& other)
		{
			rules = std::move(other.rules);
			implementation = std::move(other.implementation);
			return *this;
		}

		std::variant<std::unique_ptr<node>, error> parse(non_terminal init, std::vector<bnf::terminal_node> input)
		{
			if (table_is_old)
			{
				implementation->generate(init, rules);
				table_is_old = false;
			}

			return implementation->parse(input);
		}

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
		std::unique_ptr<tools::parser> implementation;

		std::multimap<non_terminal, std::vector<symbol>> rules;
		
		bool table_is_old = true;

		terminal t_generator = 1;
		non_terminal nt_generator = 1;
	};
}
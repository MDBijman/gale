#pragma once
#include "bnf_grammar.h"
#include "parser.h"
#include "lr_parser.h"

#include <string>
#include <map>
#include <variant>
#include <unordered_map>

namespace utils::bnf
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
		parser() : implementation(new lr::parser()) {}

		parser(parser&& other) : rules(std::move(other.rules)), implementation(std::move(other.implementation)) {}
		parser& operator=(parser&& other)
		{
			rules = std::move(other.rules);
			implementation = std::move(other.implementation);
			return *this;
		}

		void generate(non_terminal init)
		{
			if (!table_is_old)
				return;

			implementation->generate(init, rules);
			table_is_old = false;
		}

		std::variant<std::unique_ptr<node>, error> parse(std::vector<bnf::terminal_node> input)
		{
			if (table_is_old)
				throw std::runtime_error("Parser table is outdated");

			return implementation->parse(input);
		}

		parser& new_rule(const rule& r)
		{
			rules.insert(r);
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
		std::unique_ptr<utils::parser> implementation;

		std::multimap<non_terminal, std::vector<symbol>> rules;

		bool table_is_old = true;

		terminal t_generator = 1;
		non_terminal nt_generator = 1;

		// TODO check if it still breaks when removing this pragma
#pragma optimize("", off)
	};
}

#pragma once
#include "bnf_parser.h"
#include "bnf_grammar.h"
#include <functional>
#include <vector>
#include <unordered_map>

namespace utils::ebnf
{
	using terminal = bnf::terminal;
	using non_terminal = bnf::non_terminal;
	using symbol = bnf::symbol;

	constexpr terminal end_of_input = bnf::end_of_input;
	constexpr terminal epsilon = bnf::epsilon;

	enum class error_code
	{
		BNF_ERROR,
	};

	struct error
	{
		error_code type;
		std::string message;
	};

	namespace meta {
		enum meta_char {
			alt,
			lsb,
			rsb,
			lrb,
			rrb,
			star
		};

		static const char* meta_char_as_string[] = {
			"|",
			"[", "]",
			"(", ")",
			"*"
		};
	}

	using namespace meta;

	enum class child_type {
		REPETITION,
		GROUP,
		OPTIONAL
	};

	struct rule
	{
		const non_terminal lhs;
		const std::vector<std::variant<symbol, meta_char>> rhs;

		rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs);

		// #todo check if used
		bool contains_metatoken() const;

		std::vector<bnf::rule> to_bnf(std::function<non_terminal(non_terminal, child_type)> nt_generator) const;

	private:
		std::vector<bnf::rule> simplify_repetition(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const;
		std::vector<bnf::rule> simplify_group(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const;
		std::vector<bnf::rule> simplify_alt(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const;
	};

	class parser
	{
		bnf::parser bnf_parser;
		std::vector<rule> rules;

		std::unordered_map<non_terminal, std::pair<non_terminal, child_type>> nt_child_parents;

	public:
		parser() {}

		void generate(non_terminal init);

		std::variant<bnf::tree, error> parse(std::vector<bnf::terminal_node> input);

		parser& new_rule(rule r);

		terminal new_terminal();

		non_terminal new_non_terminal();

	private:
		non_terminal generate_child_non_terminal(non_terminal parent, child_type type);

		bnf::tree convert(bnf::tree);
	};
}
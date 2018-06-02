#pragma once
#include "ebnf_parser.h"

#include <functional>
#include <memory>
#include <vector>
#include <variant>

namespace utils::ebnfe
{
	using terminal = ebnf::terminal;
	using non_terminal = ebnf::non_terminal;
	using symbol = ebnf::symbol;
	using rule = ebnf::rule;

	enum class error_code
	{
		EBNF_PARSER_ERROR,
		OTHER
	};

	struct error
	{
		error_code type;
		std::string message;
	};

	enum class transformation_type
	{
		REMOVE,
		REPLACE_WITH_CHILDREN,
		KEEP,
		REMOVE_IF_ONE_CHILD,
		REPLACE_IF_ONE_CHILD
	};

	class parser
	{
	public:
		parser();

		void generate(non_terminal init);

		std::variant<bnf::tree, error> parse(std::vector<bnf::terminal_node> input);

		parser& new_transformation(std::variant<terminal, non_terminal> s, transformation_type type);

		parser& new_rule(rule r);

		terminal new_terminal();

		non_terminal new_non_terminal();

	private:
		ebnf::parser ebnf_parser;

		std::unordered_map<std::variant<terminal, non_terminal>, transformation_type> transformation_rules;

		bnf::tree convert(bnf::tree);
	};
}

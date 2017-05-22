#include "lexer_generator.h"
#include "ebnf_parser.h"
#include "lexer.h"
#include "language.h"

#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string_view>
#include <variant>
#include <iostream>

namespace lexer_generator
{
	language_lexer generate(const std::string& specification_location)
	{
		// Parse rules
		std::ifstream in(specification_location, std::ios::in | std::ios::binary);
		if (!in) throw std::exception("Could not open file");

		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		std::string_view contents_view = contents;

		language::language ebnf_language{};
		// Terminals
		auto assignment      = ebnf_language.create_terminal("::=");
		auto identifier      = ebnf_language.create_terminal("[a-zA-Z]+");
		auto alternation     = ebnf_language.create_terminal("\\|");
		auto exception       = ebnf_language.create_terminal("\\-");
		auto zero_or_more    = ebnf_language.create_terminal("\\*");
		auto one_or_more     = ebnf_language.create_terminal("\\+");
		auto terminal_string = ebnf_language.create_terminal("\'.+?\'");
		auto begin_group     = ebnf_language.create_terminal("\\(");
		auto end_group       = ebnf_language.create_terminal("\\)");
		auto end_of_rule     = ebnf_language.create_terminal("\\.");
		auto import          = ebnf_language.create_terminal("import");

		// Non terminals
		auto terminal       = ebnf_language.create_non_terminal();
		auto rhs_plus       = ebnf_language.create_non_terminal();
		auto rhs_multiplier = ebnf_language.create_non_terminal();
		auto rhs_group      = ebnf_language.create_non_terminal();
		auto rhs_exception  = ebnf_language.create_non_terminal();
		auto rhs            = ebnf_language.create_non_terminal();
		auto meta           = ebnf_language.create_non_terminal();
		auto rule           = ebnf_language.create_non_terminal();
		auto ruleset        = ebnf_language.create_non_terminal();

		auto end_of_input = ebnf_language.end_of_input;

		using namespace language::ebnf;
		ebnf_language
			.create_rule({ terminal,       { terminal_string, alt, identifier } })
			.create_rule({ rhs_plus,       { terminal, lsb, one_or_more, rsb } })
			.create_rule({ rhs_multiplier, { rhs_plus, lsb, zero_or_more, rsb, } })
			.create_rule({ rhs_group,      { rhs_multiplier, alt, begin_group, rhs_multiplier, end_group } })
			.create_rule({ rhs_exception,  { rhs_group, lsb, exception, rhs, rsb } })
			.create_rule({ rhs,            { rhs_exception, lsb, alternation, rhs, rsb, alt, terminal } });

		ebnf_language
			.create_rule({ meta, { import, identifier } });

		ebnf_language
			.create_rule({ rule,    { identifier, assignment, rhs, end_of_rule, alt, meta } })
			.create_rule({ ruleset, { rule, star, end_of_input } });

		auto tokens = ebnf_language.lex(contents);
		auto ast = ebnf_language.parse(ruleset, tokens);

		std::function<void(int, ast::node<language::symbol>*)> print_ast = [&](int indentation, ast::node<language::symbol>* node) {
			for (int i = 0; i < indentation; i++)
				std::cout << " ";
			if (node->t.is_terminal())
				std::cout << node->t.get_terminal() << std::endl;
			else
			{
				std::cout << node->t.get_non_terminal() << std::endl;
				for (auto child : node->children)
					print_ast(indentation + 1, child);
			}
		};
		print_ast(0, ast);
	}
}
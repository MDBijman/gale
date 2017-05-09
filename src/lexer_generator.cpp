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
		ebnf_language
			.define_terminal("assignment", "::=")
			.define_terminal("alternation", "\\|")
			.define_terminal("end_of_rule", "\\.")
			.define_terminal("zero_or_more", "\\*")
			.define_terminal("terminal_string", "\'.+?\'")
			.define_terminal("exception", "\\-")
			.define_terminal("begin_group", "\\(")
			.define_terminal("end_group", "\\)")
			.define_terminal("one_or_more", "\\+")
			.define_terminal("identifier", "[a-zA-Z]+");

		ebnf_language
			.define_non_terminal("rule")
			.define_non_terminal("ruleset")
			.define_non_terminal("terminal")
			.define_non_terminal("rhs_exception")
			.define_non_terminal("rhs_group")
			.define_non_terminal("rhs")
			.define_non_terminal("rhs_multiplier")
			.define_non_terminal("rhs_plus");

		ebnf_language
			.define_rule({ "terminal", { "terminal_string", '|', "identifier" } })
			.define_rule({ "rhs_plus", {"terminal", '[', "one_or_more", ']' } })
			.define_rule({ "rhs_multiplier", { "rhs_plus", '[', "zero_or_more", ']', } })
			.define_rule({ "rhs_group", { "rhs_multiplier", '|', "begin_group", "rhs_multiplier", "end_group" } })
			.define_rule({ "rhs_exception", { "rhs_group", '[', "exception", "rhs", ']' } })
			.define_rule({ "rhs", { "rhs_exception", '[', "alternation", "rhs", ']', '|', "terminal" } })
			.define_rule({ "rule", { "identifier", "assignment", "rhs", "end_of_rule", } })
			.define_rule({ "ruleset", { "rule", '*', "end_of_input" } });

		auto ebnf_lexer = ebnf_language.generate_lexer();
		auto ebnf_parser = ebnf_language.generate_parser();

		// Parse list of tokens
		auto tokens_or_error = ebnf_lexer.parse(contents);
		if (std::holds_alternative<lexing::error_code>(tokens_or_error))
		{
			std::cerr << "Error while lexing: " << (int)std::get<lexing::error_code>(tokens_or_error);
			std::cin.get();
		}
		auto tokens = std::get<std::vector<lexing::token_id>>(tokens_or_error);

		auto ast = ebnf_parser.parse(ebnf_language.to_symbol("ruleset").get_non_terminal(), tokens);

		std::function<void(int, ast::node<language::symbol>*)> print_ast = [&](int indentation, ast::node<language::symbol>* node) {
			for (int i = 0; i < indentation; i++)
				std::cout << " ";
			if (node->t.is_terminal())
				std::cout << ebnf_language.to_string(node->t.get_terminal()) << std::endl;
			else
			{
				std::cout << ebnf_language.to_string(node->t.get_non_terminal()) << std::endl;
				for (auto child : node->children)
					print_ast(indentation + 1, child);
			}
		};
		print_ast(0, ast);
	}
}
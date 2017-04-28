#include "lexer_generator.h"
#include "bnf_parser.h"
#include "lexer.h"
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

		lexer::token_definitions ebnf_tokens{
			{ "assignment", "::=" },
			{ "alternation", "\\|" },
			{ "end_of_rule", "\\." },
			{ "zero_or_more", "\\*" },
			{ "terminal_string", "\'.+?\'" },
			{ "exception", "\\-" },
			{ "begin_group", "\\(" },
			{ "end_group", "\\)" },
			{ "one_or_more", "\\+" },
			{ "identifier", "[a-zA-Z]+" },
		};

		lexer::lexer lexer{ 
			lexer::rules{
				ebnf_tokens
			}
		};

		// Parse list of tokens
		auto tokens_or_error = lexer.parse(contents);
		if (std::holds_alternative<lexer::error_code>(tokens_or_error))
		{
			std::cerr << "Error while lexing: " << (int)std::get<lexer::error_code>(tokens_or_error);
			std::cin.get();
		}
		auto tokens = std::get<std::vector<lexer::token_id>>(tokens_or_error);

		// Parse ast from token output
		bnf_parsing::symbol_definitions ebnf_symbols{ {
			{ "rule", { "identifier", "assignment", "rhs_alternation", "end_of_rule" } },

			{ "ruleset", { "rule", "ruleset" } },
			{ "ruleset", { "end_of_input" } },

			{ "terminal", { "terminal_string" } },
			{ "terminal", { "identifier" } },

			{ "primary", { "terminal", "optional_multiplier" } },
			{ "primary", { "begin_group", "rhs_alternation", "end_group" } },

			{ "rhs_exception", { "primary", "optional_exception" } },

			{ "optional_exception", { "exception", "rhs_alternation" } },
			{ "optional_exception", { "epsilon" } },

			{ "rhs_alternation", { "rhs_exception", "optional_alternation" } },

			{ "optional_multiplier", { "zero_or_more" } },
			{ "optional_multiplier", { "one_or_more" } },
			{ "optional_multiplier", { "epsilon" } },

			{ "optional_alternation", { "alternation", "rhs_alternation" } },
			{ "optional_alternation", { "epsilon" } },

			{ "zero_or_more_alternation", { "alternation", "primary", "zero_or_more_alternation" } },
			{ "zero_or_more_alternation", { "epsilon" } },
		} };


		bnf_parsing::rules rules{
			ebnf_tokens, ebnf_symbols
		};

		bnf_parsing::parser parser{
			rules		
		};

		auto ast = parser.parse("ruleset", tokens);

		std::function<void(int, ast::node<bnf_parsing::symbol>*)> print_ast = [&](int indentation, ast::node<bnf_parsing::symbol>* node) {
			for (int i = 0; i < indentation; i++)
				std::cout << " ";
			if (node->t.is_terminal())
				std::cout << rules.to_string(node->t.get_terminal()) << std::endl;
			else
			{
				std::cout << rules.to_string(node->t.get_non_terminal()) << std::endl;
				for(auto child : node->children)
					print_ast(indentation + 1, child);
			}
		};
		print_ast(0, ast);
	}
}
#pragma once
#include "ebnf_parser.h"

namespace language
{
	namespace ebnfe
	{
		class parser
		{
		public:
			parser()
			{
				// Terminals
				auto assignment = ebnf_parser.create_terminal("::=");
				auto import = ebnf_parser.create_terminal("import");
				auto identifier = ebnf_parser.create_terminal("[a-zA-Z][a-zA-Z_]+");
				auto alternation = ebnf_parser.create_terminal("\\|");
				auto terminal_string = ebnf_parser.create_terminal("\'.+?\'");
				auto begin_group = ebnf_parser.create_terminal("\\(");
				auto end_group = ebnf_parser.create_terminal("\\)");
				auto begin_repetition = ebnf_parser.create_terminal("\\{");
				auto end_repetition = ebnf_parser.create_terminal("\\}");
				auto begin_optional = ebnf_parser.create_terminal("\\[");
				auto end_optional = ebnf_parser.create_terminal("\\]");
				auto comma = ebnf_parser.create_terminal(",");
				auto semicolon = ebnf_parser.create_terminal(";");

				// Non terminals
				auto terminal = ebnf_parser.create_non_terminal();
				auto rhs = ebnf_parser.create_non_terminal();
				auto rhs_concatenation = ebnf_parser.create_non_terminal();
				auto rhs_alternation = ebnf_parser.create_non_terminal();
				auto term = ebnf_parser.create_non_terminal();
				auto meta = ebnf_parser.create_non_terminal();
				auto rule = ebnf_parser.create_non_terminal();
				auto line = ebnf_parser.create_non_terminal();
				file = ebnf_parser.create_non_terminal();
				auto end_of_input = ebnf_parser.end_of_input;

				using namespace language::ebnf;
				// Ebnf rules: these are the rules that (mostly) define ebnf
				ebnf_parser
					.create_rule({ terminal,          { terminal_string, alt, identifier } })
					.create_rule({ term,              { terminal, alt, begin_optional, rhs, end_optional, alt, begin_repetition, rhs, end_repetition, alt, begin_group, rhs, end_group } })
					.create_rule({ rhs_concatenation, { term, lsb, comma, rhs, rsb } })
					.create_rule({ rhs_alternation,   { rhs_concatenation, lsb, alternation, rhs, rsb, alt, term, comma, rhs } })
					.create_rule({ rhs,               { rhs_alternation } })
					.create_rule({ rule,              { identifier, assignment, rhs, semicolon } });

				// Meta rules: these are the rules that are extensions on top of ebnf
				ebnf_parser
					.create_rule({ meta, { import, identifier, semicolon } });

				// Combines meta and ebnf rules
				ebnf_parser
					.create_rule({ line, { rule, alt, meta } })
					.create_rule({ file, { line, star, end_of_input } });
			}

			void parse(const std::string& contents)
			{
				auto tokens_or_error = lexing::lexer{ ebnf_parser.token_rules }.parse(contents);
				auto tokens = std::get<std::vector<lexing::token_id>>(tokens_or_error);

				auto ast_or_error = ebnf_parser.parse(file, tokens);
				auto ast = std::get<ast::node<bnf::symbol>*>(ast_or_error);

				std::function<void(int, ast::node<language::bnf::symbol>*)> print_ast = [&](int indentation, ast::node<language::bnf::symbol>* node) {
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

		private:
			ebnf::non_terminal file;
			ebnf::parser ebnf_parser;
		};
	}
}
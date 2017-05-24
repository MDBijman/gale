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
				auto identifier = ebnf_parser.create_terminal("[a-zA-Z][a-zA-Z_]+");
				auto alternation = ebnf_parser.create_terminal("\\|");
				auto exception = ebnf_parser.create_terminal("\\-");
				auto zero_or_more = ebnf_parser.create_terminal("\\*");
				auto one_or_more = ebnf_parser.create_terminal("\\+");
				auto terminal_string = ebnf_parser.create_terminal("\'.+?\'");
				auto begin_group = ebnf_parser.create_terminal("\\(");
				auto end_group = ebnf_parser.create_terminal("\\)");
				auto end_of_rule = ebnf_parser.create_terminal("\\.");
				auto import = ebnf_parser.create_terminal("import");
				auto left_curly_bracket = ebnf_parser.create_terminal("\\{");
				auto right_curly_bracket = ebnf_parser.create_terminal("\\}");
				auto comma = ebnf_parser.create_terminal(",");
				auto semicolon = ebnf_parser.create_terminal(";");
				auto left_square_bracket = ebnf_parser.create_terminal("\\[");
				auto right_square_bracket = ebnf_parser.create_terminal("\\]");

				// Non terminals
				auto terminal = ebnf_parser.create_non_terminal();
				auto rhs_plus = ebnf_parser.create_non_terminal();
				auto rhs_multiplier = ebnf_parser.create_non_terminal();
				auto rhs_group = ebnf_parser.create_non_terminal();
				auto rhs_exception = ebnf_parser.create_non_terminal();
				auto rhs = ebnf_parser.create_non_terminal();
				auto meta = ebnf_parser.create_non_terminal();
				auto rule = ebnf_parser.create_non_terminal();
				auto expression = ebnf_parser.create_non_terminal();
				ruleset = ebnf_parser.create_non_terminal();

				auto end_of_input = ebnf_parser.end_of_input;

				using namespace language::ebnf;
				// Ebnf rules: these are the rules that (mostly) define ebnf
				ebnf_parser
					.create_rule({ terminal,       { terminal_string, alt, identifier } })
					.create_rule({ rhs_plus,       { terminal, lsb, one_or_more, rsb } })
					.create_rule({ rhs_multiplier, { rhs_plus, lsb, zero_or_more, rsb, } })
					.create_rule({ rhs_group,      { rhs_multiplier, alt, begin_group, rhs_multiplier, end_group } })
					.create_rule({ rhs_exception,  { rhs_group, lsb, exception, rhs, rsb } })
					.create_rule({ rhs,            { rhs_exception, lsb, alternation, rhs, rsb, alt, terminal } })
					.create_rule({ rule,           { identifier, assignment, rhs, semicolon } });

				// Meta rules: these are the rules that are extensions on top of ebnf
				ebnf_parser
					.create_rule({ meta, { import, identifier, semicolon } });

				// Combines meta and ebnf rules
				ebnf_parser
					.create_rule({ expression, { rule, alt, meta } })
					.create_rule({ ruleset, { expression, star, end_of_input } });
			}

			void parse(const std::string& contents)
			{
				auto tokens = ebnf_parser.lex(contents);
					auto ast = ebnf_parser.parse(ruleset, tokens);

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
				//print_ast(0, ast);
			}

		private:
			ebnf::non_terminal ruleset;
			ebnf::parser ebnf_parser;
		};
	}
}
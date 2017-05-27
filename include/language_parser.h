#pragma once
#include "lexer.h"
#include "ebnfe_parser.h"

namespace language
{
	namespace fe
	{
		class lexer_parser_mapper
		{
		public:
			void add_mapping(lexing::token_id token, std::variant<ebnf::terminal, std::function<ebnf::terminal(lexing::token)>> converter)
			{
				mapping.insert({ token, converter });
			}

			std::vector<ebnf::terminal> convert(std::vector<lexing::token> input)
			{
				std::vector<ebnf::terminal> result;
				std::transform(input.begin(), input.end(), std::back_inserter(result), [&](auto& x) {
					std::variant<ebnf::terminal, std::function<ebnf::terminal(lexing::token)>> mapped = mapping.at(x.value);
					if (std::holds_alternative<ebnf::terminal>(mapped))
						return std::get<ebnf::terminal>(mapped);
					else
						return std::get<std::function<ebnf::terminal(lexing::token)>>(mapped)(x);
				});
				return result;
			}

			std::unordered_map<lexing::token_id, std::variant<ebnf::terminal, std::function<ebnf::terminal(lexing::token)>>> mapping;
		};

		class parser
		{
		public:
			parser() : lexer(lexing::rules{})
			{
				// Terminals
				auto lexing_rules = lexing::rules{};
				auto assignment_token = lexing_rules.create_token("::=");
				auto keyword_token = lexing_rules.create_token("[a-zA-Z][a-zA-Z_]*");
				auto alt_token = lexing_rules.create_token("\\|");
				auto string_token = lexing_rules.create_token("\'.+?\'");
				auto lrb_token = lexing_rules.create_token("\\(");
				auto rrb_token = lexing_rules.create_token("\\)");
				auto lcb_token = lexing_rules.create_token("\\{");
				auto rcb_token = lexing_rules.create_token("\\}");
				auto lsb_token = lexing_rules.create_token("\\[");
				auto rsb_token = lexing_rules.create_token("\\]");
				auto comma_token = lexing_rules.create_token(",");
				auto semicolon_token = lexing_rules.create_token(";");
				lexer = lexing::lexer{ lexing_rules };

				auto assignment = ebnfe_parser.new_terminal("assignment");
				auto import = ebnfe_parser.new_terminal();
				auto identifier = ebnfe_parser.new_terminal("identifier");
				auto alternation = ebnfe_parser.new_terminal();
				auto terminal_string = ebnfe_parser.new_terminal();
				auto begin_group = ebnfe_parser.new_terminal();
				auto end_group = ebnfe_parser.new_terminal();
				auto begin_repetition = ebnfe_parser.new_terminal();
				auto end_repetition = ebnfe_parser.new_terminal();
				auto begin_optional = ebnfe_parser.new_terminal();
				auto end_optional = ebnfe_parser.new_terminal();
				auto comma = ebnfe_parser.new_terminal();
				auto semicolon = ebnfe_parser.new_terminal();

				// Lexer to parser mappings
				{
					mapper.add_mapping(assignment_token, assignment);
					mapper.add_mapping(keyword_token, [import, identifier](auto& token) {
						if (token.string == "import")
							return import;
						return identifier;
					});
					mapper.add_mapping(alt_token, alternation);
					mapper.add_mapping(string_token, terminal_string);
					mapper.add_mapping(lrb_token, begin_group);
					mapper.add_mapping(rrb_token, end_group);
					mapper.add_mapping(lcb_token, begin_repetition);
					mapper.add_mapping(rcb_token, end_repetition);
					mapper.add_mapping(lsb_token, begin_optional);
					mapper.add_mapping(rsb_token, end_optional);
					mapper.add_mapping(comma_token, comma);
					mapper.add_mapping(semicolon_token, semicolon);
				}

				// Non terminals
				auto terminal = ebnfe_parser.new_non_terminal();
				auto rhs_alternation = ebnfe_parser.new_non_terminal();
				auto rhs_concatenation = ebnfe_parser.new_non_terminal();
				auto term = ebnfe_parser.new_non_terminal();
				auto meta = ebnfe_parser.new_non_terminal();
				auto rule = ebnfe_parser.new_non_terminal();
				auto line = ebnfe_parser.new_non_terminal();
				auto optional = ebnfe_parser.new_non_terminal();
				auto repetition = ebnfe_parser.new_non_terminal();
				auto grouping = ebnfe_parser.new_non_terminal();
				file = ebnfe_parser.new_non_terminal();
				auto end_of_input = ebnf::end_of_input;

				using namespace language::ebnf;
				// Ebnf rules: these are the rules that (mostly) define ebnf
				ebnfe_parser
					.create_rule({ terminal,          { terminal_string, alt, identifier } })
					.create_rule({ optional,          { begin_optional, rhs_alternation, end_optional } })
					.create_rule({ repetition,        { begin_repetition, rhs_alternation, end_repetition } })
					.create_rule({ grouping,          { begin_group, rhs_alternation, end_group } })
					.create_rule({ term,              { terminal, alt, optional, alt, repetition, alt, grouping } })
					.create_rule({ rhs_concatenation, { term, lsb, comma, rhs_alternation, rsb } })
					.create_rule({ rhs_alternation,   { rhs_concatenation, lsb, alternation, rhs_alternation, rsb } })
					.create_rule({ rule,              { identifier, assignment, rhs_alternation, semicolon } });

				// Meta rules: these are the rules that are extensions on top of ebnf
				ebnfe_parser
					.create_rule({ meta, { import, identifier, semicolon } });

				// Combines meta and ebnf rules
				ebnfe_parser
					.create_rule({ line, { rule, alt, meta } })
					.create_rule({ file, { line, star, end_of_input } });

				ebnfe_parser
					.add_transformation(term, ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
					.add_transformation(comma, ebnfe::transformation_type::REMOVE)
					.add_transformation(begin_optional, ebnfe::transformation_type::REMOVE)
					.add_transformation(end_optional, ebnfe::transformation_type::REMOVE)
					.add_transformation(begin_repetition, ebnfe::transformation_type::REMOVE)
					.add_transformation(end_repetition, ebnfe::transformation_type::REMOVE)
					.add_transformation(rhs_alternation, ebnfe::transformation_type::REMOVE_IF_ONE_CHILD)
					.add_transformation(rhs_concatenation, ebnfe::transformation_type::REMOVE_IF_ONE_CHILD);
			}

			void parse(const std::string& contents)
			{
				auto tokens_or_error = lexer.parse(contents);
				auto tokens = std::get<std::vector<lexing::token>>(tokens_or_error);

				auto parser_input = mapper.convert(tokens);

				auto ast_or_error = ebnfe_parser.parse(file, parser_input);
				auto ast = std::get<ebnfe::node*>(ast_or_error);

			}

			/*

			void prune(ast::node<symbol> tree) const;
				void parser::prune(ast::node<symbol> tree) const
				{
					for (auto child : tree.children)
					{
						this->prune(*child);
					}

					tree.children.erase(
						std::remove_if(tree.children.begin(), tree.children.end(), [](auto& child) {
							if (child->value.is_terminal() && (child->value.get_terminal() == epsilon) ||
								(!child->value.is_terminal() && child->is_leaf()))
							{
								return true;
							}
							return false;
						}),
						tree.children.end()
					);
				}
			*/

		private:
			ebnfe::non_terminal file;
			ebnfe::parser ebnfe_parser;
			lexing::lexer lexer;

			lexer_parser_mapper mapper;
		};
	}
}
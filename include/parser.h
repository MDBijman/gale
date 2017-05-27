#pragma once
#include "lexer.h"
#include "ebnfe_parser.h"

namespace language
{
	namespace fe
	{
		ebnfe::non_terminal
			file, statement, assignment, print;

		ebnfe::terminal
			equals, identifier, number, print_keyword, semicolon;

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
				auto assignment_token = lexing_rules.create_token("=");
				auto word_token = lexing_rules.create_token("[a-zA-Z][a-zA-Z_]*");
				auto number_token = lexing_rules.create_token("[1-9][0-9]*");
				auto semicolon_token = lexing_rules.create_token(";");
				lexer = lexing::lexer{ lexing_rules };

				equals = ebnfe_parser.new_terminal();
				identifier = ebnfe_parser.new_terminal();
				number = ebnfe_parser.new_terminal();
				print_keyword = ebnfe_parser.new_terminal();
				semicolon = ebnfe_parser.new_terminal();

				mapper.add_mapping(assignment_token, equals);
				mapper.add_mapping(word_token, [](lexing::token token) {
					if (token.string == "print")
						return print_keyword;

					return identifier;
				});
				mapper.add_mapping(number_token, number);
				mapper.add_mapping(semicolon_token, semicolon);

				file = ebnfe_parser.new_non_terminal();
				statement = ebnfe_parser.new_non_terminal();
				assignment = ebnfe_parser.new_non_terminal();
				print = ebnfe_parser.new_non_terminal();

				using namespace ebnf::meta;
				ebnfe_parser
					.new_rule({ assignment, { identifier, equals, number } })
					.new_rule({ print, { print_keyword, identifier } })
					.new_rule({ statement, { assignment, semicolon, alt, print, semicolon } })
					.new_rule({ file, { statement, star } });

				ebnfe_parser
					.new_transformation(semicolon, ebnfe::transformation_type::REMOVE)
					.new_transformation(statement, ebnfe::transformation_type::REPLACE_WITH_CHILDREN)
					.new_transformation(number, ebnfe::transformation_type::REMOVE)
					.new_transformation(print_keyword, ebnfe::transformation_type::REMOVE);
			}

			ebnfe::node* parse(const std::string& contents)
			{
				auto tokens_or_error = lexer.parse(contents);
				auto tokens = std::get<std::vector<lexing::token>>(tokens_or_error);
				auto parser_input = mapper.convert(tokens);
				auto ast_or_error = ebnfe_parser.parse(file, parser_input);
				return std::get<ebnfe::node*>(ast_or_error);
			}

		private:
			ebnfe::parser ebnfe_parser;
			lexing::lexer lexer;

			lexer_parser_mapper mapper;
		};
	}
}

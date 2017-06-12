#pragma once
#include "ebnfe_parser.h"
#include "lexer.h"
#include "language.h"
#include "pipeline.h"

namespace fe
{
	class lexer_to_parser_stage : public language::lexer_to_parser_stage<tools::lexing::token, tools::ebnfe::terminal>
	{
	public:
		lexer_to_parser_stage()
		{
			using namespace fe;
			add_mapping(assignment_token, equals);
			add_mapping(word_token, [](tools::lexing::token token) {
				if (token.string == "print")
					return print_keyword;

				return identifier;
			});
			add_mapping(number_token, number);
			add_mapping(semicolon_token, semicolon);
		}

		std::vector<tools::ebnfe::terminal> convert(const std::vector<tools::lexing::token>& in)
		{
			std::vector<tools::ebnfe::terminal> result;
			std::transform(in.begin(), in.end(), std::back_inserter(result), [&](tools::lexing::token x) {
				std::variant<tools::ebnf::terminal, std::function<tools::ebnf::terminal(tools::lexing::token)>> mapped = mapping.at(x.value);
				if (std::holds_alternative<tools::ebnf::terminal>(mapped))
					return std::get<tools::ebnf::terminal>(mapped);
				else
					return std::get<std::function<tools::ebnf::terminal(tools::lexing::token)>>(mapped)(x);
			});
			return result;
		}

	private:
		void add_mapping(tools::lexing::token_id token, std::variant<tools::ebnfe::terminal, std::function<tools::ebnfe::terminal(tools::lexing::token)>> converter)
		{
			mapping.insert({ token, converter });
		}

		std::unordered_map<tools::lexing::token_id, std::variant<tools::ebnfe::terminal, std::function<tools::ebnfe::terminal(tools::lexing::token)>>> mapping;

	};
}

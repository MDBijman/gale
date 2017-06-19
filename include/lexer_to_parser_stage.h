#pragma once
#include "ebnfe_parser.h"
#include "lexer.h"
#include "language.h"
#include "pipeline.h"

namespace fe
{
	class lexer_to_parser_stage : public language::lexer_to_parser_stage<tools::lexing::token, tools::bnf::terminal_node>
	{
	public:
		lexer_to_parser_stage()
		{
			add_mapping(string_token, word);
			add_mapping(number_token, number);
			add_mapping(lrb_token, left_bracket);
			add_mapping(rrb_token, right_bracket);
			add_mapping(equals_token, equals);
			add_mapping(keyword_token, identifier);
		}

		std::vector<tools::bnf::terminal_node> convert(const std::vector<tools::lexing::token>& in)
		{
			std::vector<tools::bnf::terminal_node> result;

			std::transform(in.begin(), in.end(), std::back_inserter(result), [&](tools::lexing::token x) {

				std::variant<tools::ebnf::terminal, std::function<tools::ebnf::terminal(tools::lexing::token)>> mapped = mapping.at(x.value);

				if (std::holds_alternative<tools::ebnf::terminal>(mapped))
					return tools::bnf::terminal_node(std::get<tools::bnf::terminal>(mapped), x.text);
				else
					return tools::bnf::terminal_node(std::get<std::function<tools::bnf::terminal(tools::lexing::token)>>(mapped)(x), x.text);

			});
			return result;
		}

	private:
		void add_mapping(tools::lexing::token_id token, std::variant<tools::bnf::terminal, std::function<tools::bnf::terminal(tools::lexing::token)>> converter)
		{
			mapping.insert({ token, converter });
		}

		std::unordered_map<tools::lexing::token_id, std::variant<tools::bnf::terminal, std::function<tools::bnf::terminal(tools::lexing::token)>>> mapping;

	};
}

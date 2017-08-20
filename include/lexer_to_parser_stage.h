#pragma once
#include "ebnfe_parser.h"
#include "lexer.h"
#include "language_definition.h"
#include "pipeline.h"

namespace fe
{
	struct lex_to_parse_error
	{

	};

	class lexer_to_parser_stage : public language::lexer_to_parser_stage<tools::lexing::token, tools::bnf::terminal_node, lex_to_parse_error>
	{
	public:
		lexer_to_parser_stage()
		{
			add_mapping(string_token, word);
			add_mapping(number_token, number);
			add_mapping(lrb_token, left_bracket);
			add_mapping(rrb_token, right_bracket);
			add_mapping(equals_token, equals);
			add_mapping(module_infix_token, module_infix);
			add_mapping(plus_token, plus);
			add_mapping(minus_token, minus);
			add_mapping(multiply_token, times);
			add_mapping(divide_token, divide);
			add_mapping(keyword_token, [&](tools::lexing::token token) {
				if (token.text == "export")
					return export_keyword;
				if (token.text == "type")
					return type_keyword;
				return identifier;
			});
		}

		std::variant<std::vector<tools::bnf::terminal_node>, lex_to_parse_error> convert(const std::vector<tools::lexing::token>& in)
		{
			using namespace tools;

			std::vector<bnf::terminal_node> result;

			std::transform(in.begin(), in.end(), std::back_inserter(result), [&](lexing::token x) 
			{
				std::variant<ebnf::terminal, std::function<ebnf::terminal(lexing::token)>>& mapped = mapping.at(x.value);

				if (std::holds_alternative<ebnf::terminal>(mapped))
				{
					return bnf::terminal_node(std::get<bnf::terminal>(mapped), x.text);
				}
				else
				{
					return bnf::terminal_node(std::get<std::function<bnf::terminal(lexing::token)>>(mapped)(x), x.text);
				}
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

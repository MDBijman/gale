#pragma once
#include "utils/lexing/lexer.h"
#include "utils/parsing/ebnfe_parser.h"
#include "fe/pipeline/error.h"
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"

namespace fe
{
	class lexer_to_parser_stage 
	{
	public:
		lexer_to_parser_stage()
		{
			using namespace terminals;
			using namespace tokens;

			add_mapping(string_token, word);
			add_mapping(number_token, number);
			add_mapping(lrb_token, left_bracket);
			add_mapping(rrb_token, right_bracket);
			add_mapping(lcb_token, left_curly_bracket);
			add_mapping(rcb_token, right_curly_bracket); 
			add_mapping(lsb_token, left_square_bracket);
			add_mapping(rsb_token, right_square_bracket);
			add_mapping(lab_token, left_angle_bracket);
			add_mapping(rab_token, right_angle_bracket);
			add_mapping(pipe_token, vertical_line);
			add_mapping(right_arrow_token, right_arrow);
			add_mapping(equals_token, equals);
			add_mapping(comma_token, comma);
			add_mapping(semicolon_token, semicolon);
			add_mapping(mul_token, mul);
			add_mapping(div_token, div);
			add_mapping(plus_token, plus);
			add_mapping(minus_token, minus);
			add_mapping(colon_token, colon);
			add_mapping(dot_token, dot);
			add_mapping(equality_token, two_equals);
			add_mapping(percentage_token, percentage);
			add_mapping(lteq_token, lteq);
			add_mapping(keyword_token, [&](utils::lexing::token token) {
				if (token.text == "export")
					return export_keyword;
				if (token.text == "type")
					return type_keyword;
				if (token.text == "fn")
					return function_keyword;
				if (token.text == "match")
					return match_keyword;
				if (token.text == "module")
					return module_keyword;
				if (token.text == "pub")
					return public_keyword;
				if (token.text == "ref")
					return ref_keyword;
				if (token.text == "var")
					return var_keyword;
				if (token.text == "import")
					return import_keyword;
				if (token.text == "qualified")
					return qualified_keyword;
				if (token.text == "as")
					return as_keyword;
				if (token.text == "from")
					return from_keyword;
				if (token.text == "while")
					return while_keyword;
				if (token.text == "do")
					return do_keyword;
				if (token.text == "on")
					return on_keyword;
				if (token.text == "true")
					return true_keyword;
				if (token.text == "false")
					return false_keyword;
				return identifier;
			});
		}

		std::variant<std::vector<utils::bnf::terminal_node>, lex_to_parse_error> convert(const std::vector<utils::lexing::token>& in) const
		{
			using namespace utils;

			std::vector<bnf::terminal_node> result;

			std::transform(in.begin(), in.end(), std::back_inserter(result), [&](lexing::token x) 
			{
				const std::variant<ebnf::terminal, std::function<ebnf::terminal(lexing::token)>>& mapped = mapping.at(x.value);

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
		void add_mapping(utils::lexing::token_id token, std::variant<utils::bnf::terminal, std::function<utils::bnf::terminal(utils::lexing::token)>> converter)
		{
			mapping.insert({ token, converter });
		}

		std::unordered_map<utils::lexing::token_id, std::variant<utils::bnf::terminal, std::function<utils::bnf::terminal(utils::lexing::token)>>> mapping;
	};
}

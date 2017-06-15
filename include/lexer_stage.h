#pragma once
#include "pipeline.h"
#include "language.h"
#include "lexer.h"

namespace fe
{
	class lexing_stage : public language::lexing_stage<tools::lexing::token>
	{
	public:
		lexing_stage() : lexer(ruleset())
		{
		}

		std::vector<tools::lexing::token> lex(const std::string& in) override
		{
			return std::get<std::vector<tools::lexing::token>>(lexer.parse(in));
		}

		tools::lexing::rules ruleset()
		{
			tools::lexing::rules lexing_rules;

			using namespace fe;
			assignment_token = lexing_rules.create_token("=");
			word_token = lexing_rules.create_token("[a-zA-Z][a-zA-Z_]*");
			number_token = lexing_rules.create_token("[1-9][0-9]*");
			semicolon_token = lexing_rules.create_token(";");
			lcb_token = lexing_rules.create_token("\\{");
			rcb_token = lexing_rules.create_token("\\}");
			lexing_rules.compile();
			return lexing_rules;
		}

	private:
		tools::lexing::lexer lexer;
	};
}
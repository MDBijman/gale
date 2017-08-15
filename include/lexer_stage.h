#pragma once
#include "pipeline.h"
#include "language_definition.h"
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
			string_token = lexing_rules.create_token("\".*?\"");
			number_token = lexing_rules.create_token("[1-9][0-9]*");
			plus_token = lexing_rules.create_token("\\+");
			minus_token = lexing_rules.create_token("\\-");
			module_infix_token = lexing_rules.create_token("::");
			rrb_token = lexing_rules.create_token("\\)");
			lrb_token = lexing_rules.create_token("\\(");
			equals_token = lexing_rules.create_token("=");
			keyword_token = lexing_rules.create_token("[a-zA-Z][a-zA-Z0-9_:]*");
			lexing_rules.compile();
			return lexing_rules;
		}

	private:
		tools::lexing::lexer lexer;
	};
}
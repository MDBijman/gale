#pragma once
#include "pipeline.h"
#include "language_definition.h"
#include "lexer.h"

namespace fe
{
	class lexing_stage : public language::lexing_stage<tools::lexing::token, tools::lexing::error>
	{
	public:
		lexing_stage() : lexer(ruleset())
		{
		}

		std::variant<std::vector<tools::lexing::token>, tools::lexing::error> lex(const std::string& in) override
		{
			return lexer.parse(in);
		}

		tools::lexing::rules ruleset()
		{
			tools::lexing::rules lexing_rules;

			using namespace fe;
			string_token = lexing_rules.create_token("\".*?\"");
			number_token = lexing_rules.create_token("\\-?[1-9][0-9]*|0");
			right_arrow_token = lexing_rules.create_token("\\->");
			module_infix_token = lexing_rules.create_token("::");
			rrb_token = lexing_rules.create_token("\\)");
			lrb_token = lexing_rules.create_token("\\(");
			rcb_token = lexing_rules.create_token("\\}");
			lcb_token = lexing_rules.create_token("\\{");
			lsb_token = lexing_rules.create_token("\\[");
			rsb_token = lexing_rules.create_token("\\]");
			pipe_token = lexing_rules.create_token("\\|");
			comma_token = lexing_rules.create_token(",");
			equals_token = lexing_rules.create_token("=");
			keyword_token = lexing_rules.create_token("[a-zA-Z][a-zA-Z0-9_:]*");
			lexing_rules.compile();
			return lexing_rules;
		}

	private:
		tools::lexing::lexer lexer;
	};
}
#pragma once
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "utils/lexing/lexer.h"

namespace fe
{
	class lexing_stage 
	{
	public:
		lexing_stage() : lexer(ruleset())
		{
		}

		std::variant<std::vector<utils::lexing::token>, utils::lexing::error> lex(const std::string& in) const
		{
			return lexer.parse(in);
		}

		static utils::lexing::rules ruleset()
		{
			utils::lexing::rules lexing_rules;

			using namespace fe::tokens;
			string_token = lexing_rules.create_token("\".*?\"");
			number_token = lexing_rules.create_token("\\-?[1-9][0-9]*|0");
			right_arrow_token = lexing_rules.create_token("\\->");
			fat_right_arrow_token = lexing_rules.create_token("=>");
			rrb_token = lexing_rules.create_token("\\)");
			lrb_token = lexing_rules.create_token("\\(");
			rcb_token = lexing_rules.create_token("\\}");
			lcb_token = lexing_rules.create_token("\\{");
			lsb_token = lexing_rules.create_token("\\[");
			rsb_token = lexing_rules.create_token("\\]");
			lteq_token = lexing_rules.create_token("<=");
			gteq_token = lexing_rules.create_token(">=");
			lab_token = lexing_rules.create_token("<");
			rab_token = lexing_rules.create_token(">");
			or_token = lexing_rules.create_token("\\|\\|");
			pipe_token = lexing_rules.create_token("\\|");
			comma_token = lexing_rules.create_token(",");
			equality_token = lexing_rules.create_token("==");
			equals_token = lexing_rules.create_token("=");
			keyword_token = lexing_rules.create_token("[a-zA-Z_][a-zA-Z0-9_.]*");
			semicolon_token = lexing_rules.create_token(";");
			plus_token = lexing_rules.create_token("\\+");
			minus_token = lexing_rules.create_token("\\-");
			mul_token = lexing_rules.create_token("\\*");
			div_token = lexing_rules.create_token("/");
			colon_token = lexing_rules.create_token(":");
			dot_token = lexing_rules.create_token("\\.");
			percentage_token = lexing_rules.create_token("%");
			backslash_token = lexing_rules.create_token("\\\\");
			and_token = lexing_rules.create_token("&&");
			lexing_rules.compile();
			return lexing_rules;
		}

	private:
		utils::lexing::lexer lexer;
	};
}
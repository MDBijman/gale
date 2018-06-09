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

		std::optional<lexing::error> lex(const std::string& in, lexing::token_stream_writer writer) const
		{
			return lexer.parse(in, writer);
		}

		static lexing::rules ruleset()
		{
			lexing::rules lexing_rules;

			using namespace fe::tokens;
			right_arrow = lexing_rules.create_token("->");
			fat_right_arrow = lexing_rules.create_token("=>");
			left_bracket = lexing_rules.create_token("(");
			right_bracket = lexing_rules.create_token(")");
			left_curly_bracket = lexing_rules.create_token("{");
			right_curly_bracket = lexing_rules.create_token("}");
			left_square_bracket = lexing_rules.create_token("[");
			right_square_bracket = lexing_rules.create_token("]");
			left_angle_bracket = lexing_rules.create_token("<");
			right_angle_bracket = lexing_rules.create_token(">");
			lteq = lexing_rules.create_token("<=");
			gteq = lexing_rules.create_token(">=");
			or_keyword = lexing_rules.create_token("||");
			and_keyword = lexing_rules.create_token("&&");
			vertical_line = lexing_rules.create_token("|");
			comma = lexing_rules.create_token(",");
			two_equals = lexing_rules.create_token("==");
			equals = lexing_rules.create_token("=");
			semicolon = lexing_rules.create_token(";");
			plus = lexing_rules.create_token("+");
			minus = lexing_rules.create_token("-");
			mul = lexing_rules.create_token("*");
			div = lexing_rules.create_token("/");
			colon = lexing_rules.create_token(":");
			dot = lexing_rules.create_token(".");
			percentage = lexing_rules.create_token("%");
			backslash = lexing_rules.create_token("\\");
			type_keyword = lexing_rules.create_token("type");
			match_keyword = lexing_rules.create_token("match");
			module_keyword = lexing_rules.create_token("module");
			public_keyword = lexing_rules.create_token("public");
			ref_keyword = lexing_rules.create_token("ref");
			let_keyword = lexing_rules.create_token("let");
			import_keyword = lexing_rules.create_token("import");
			while_keyword = lexing_rules.create_token("while");
			true_keyword = lexing_rules.create_token("true");
			false_keyword = lexing_rules.create_token("false");
			if_keyword = lexing_rules.create_token("if");
			elseif_keyword = lexing_rules.create_token("elseif");
			else_keyword = lexing_rules.create_token("else");
			word = lexing_rules.create_string_token();
			tokens::number = lexing_rules.create_number_token();
			identifier = lexing_rules.create_identifier_token();
			return lexing_rules;
		}

	private:
		lexing::lexer lexer;
	};
}
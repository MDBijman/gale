#pragma once
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "utils/lexing/lexer.h"

namespace fe
{
	class lexing_stage 
	{
	public:
		std::variant<lexing::error, std::vector<lexing::token>> lex(const std::string& in) const
		{
			return lexer.parse(in);
		}

	private:
		lexing::lexer lexer;
	};
}
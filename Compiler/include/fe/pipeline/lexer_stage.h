#pragma once
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "utils/lexing/lexer.h"

namespace fe
{
	class lexing_stage 
	{
	public:
		std::optional<lexing::error> lex(const std::string& in, lexing::token_stream_writer writer) const
		{
			return lexer.parse(in, writer);
		}

	private:
		lexing::lexer lexer;
	};
}
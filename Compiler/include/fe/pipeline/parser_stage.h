#pragma once
#include "fe/language_definition.h"
#include "fe/data/ext_ast.h"
#include "fe/pipeline/error.h"

#include <memory>

namespace fe
{
	class parsing_stage 
	{
	public:
		parsing_stage();
		std::variant<ext_ast::ast, parse_error> parse(const std::vector<utils::lexing::token>& in);
	};
}
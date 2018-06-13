#pragma once
#include "fe/language_definition.h"
#include "fe/data/ext_ast.h"
#include "fe/pipeline/error.h"
#include "utils/parsing/recursive_descent_parser.h"

#include <memory>

namespace fe
{
	class parsing_stage 
	{
	public:
		parsing_stage();
		std::variant<ext_ast::ast, parse_error> parse(std::vector<lexing::token>& in);
	};
}
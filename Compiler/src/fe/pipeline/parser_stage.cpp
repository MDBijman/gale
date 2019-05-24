#include "fe/pipeline/parser_stage.h"
#include "utils/parsing/recursive_descent_parser.h"

namespace fe
{
	parsing_stage::parsing_stage()
	{
		recursive_descent::generate();
	}

	std::variant<ext_ast::ast, parse_error> parsing_stage::parse(std::vector<lexing::token> &in)
	{
		auto res = recursive_descent::parse(in);
		if (std::holds_alternative<ext_ast::ast>(res))
			return std::move(std::get<ext_ast::ast>(res));
		else
			return parse_error(std::get<recursive_descent::error>(res).message);
	}
} // namespace fe
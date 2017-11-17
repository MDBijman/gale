#pragma once

namespace fe
{
	struct cst_to_ast_error
	{
		std::string message;
	};

	struct lex_to_parse_error
	{
		std::string message;
	};

	struct typecheck_error
	{
		std::string message;
	};

	struct lower_error
	{
		std::string message;
	};

	struct interp_error
	{
		std::string message;
	};

	struct type_env_error
	{
		std::string message;
	};

	struct value_env_error
	{
		std::string message;
	};
}
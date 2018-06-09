#pragma once
#include <string>

namespace fe
{
	struct typecheck_error
	{
		std::string message;
	};

	struct lower_error
	{
		std::string message;
	};

	struct parse_error
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

	struct resolution_error
	{
		std::string message;
	};

	struct other_error
	{
		std::string message;
	};
}
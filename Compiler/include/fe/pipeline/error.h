#pragma once
#include <string>

namespace fe
{
	struct error
	{
		virtual ~error() = 0;
		virtual std::string what() const = 0;
	};

	struct typecheck_error : public error
	{
		typecheck_error(std::string msg) : message(msg)
		{
		}

		std::string message;

		virtual std::string what() const
		{
			return "Typechecking error: " + message;
		}
	};

	struct lower_error : public error
	{
		lower_error(std::string msg) : message(msg)
		{
		}

		std::string message;

		virtual std::string what() const
		{
			return "Lowering error: " + message;
		}
	};

	struct parse_error : public error
	{
		parse_error(std::string msg) : message(msg)
		{
		}

		std::string message;

		virtual std::string what() const
		{
			return "Parse error: " + message;
		}
	};

	struct resolution_error : public error
	{
		resolution_error(std::string msg) : message(msg)
		{
		}

		std::string message;

		virtual std::string what() const
		{
			return "Resolution error: " + message;
		}
	};

	struct other_error : public error
	{
		other_error(std::string msg) : message(msg)
		{
		}

		std::string message;

		virtual std::string what() const
		{
			return message;
		}
	};
} // namespace fe
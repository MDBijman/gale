#pragma once
#include <string>
#include <variant>

namespace utils
{
	namespace files
	{
		std::variant<std::string, std::exception> read_file(const std::string& name);
	}
}
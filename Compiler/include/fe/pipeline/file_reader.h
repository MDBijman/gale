#pragma once
#include <optional>
#include <string>

namespace fe
{
	std::optional<std::string> read_file(const std::string &filename);
} // namespace fe

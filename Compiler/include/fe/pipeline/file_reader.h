#pragma once
#include <filesystem>
#include <optional>
#include <string>

#include "utils/reading/reader.h"

namespace fe
{
	std::optional<std::string> read_file(const std::string &filename)
	{
		auto file_path = std::filesystem::path(filename);
		auto file_or_error = utils::files::read_file(file_path.string());

		if (std::holds_alternative<std::exception>(file_or_error))
            return std::nullopt;

		auto &code = std::get<std::string>(file_or_error);
        return code;
	}
} // namespace fe

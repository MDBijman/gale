#include "utils/reading/reader.h"

#include <fstream>

namespace utils
{
	namespace files
	{
		std::variant<std::string, std::exception> read_file(const std::string& name)
		{
			std::ifstream in(name, std::ios::in | std::ios::binary);
			if (!in) return std::exception();

			std::string contents;
			in.seekg(0, std::ios::end);
			contents.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&contents[0], contents.size());
			in.close();

			return contents;
		}
	}
}
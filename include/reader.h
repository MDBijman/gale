#pragma once
#include <string>
#include <fstream>

namespace tools
{
	namespace files
	{
		std::string read_file(const std::string& name)
		{
			std::ifstream in(name, std::ios::in | std::ios::binary);
			if (!in) throw std::exception("Could not open file");

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
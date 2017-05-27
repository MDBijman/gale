#include "language_parser.h"
#include <stdio.h>
#include <iostream>
#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string_view>
#include <variant>
#include <iostream>

void generate(const std::string& specification_location)
{

}

int main()
{
	// Parse rules
	std::ifstream in("./snippets/tiny.ebnfe", std::ios::in | std::ios::binary);
	if (!in) throw std::exception("Could not open file");

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();
	std::string_view contents_view = contents;

	language::fe::parser ebnfe_parser;
	ebnfe_parser.parse(contents);

	std::cin.get();
}
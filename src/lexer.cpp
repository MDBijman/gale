#include "lexer.h"
#include <fstream>
#include <string>
#include <cerrno>

namespace lexer
{
	void combination::add_definition(lexical_definition* d)
	{
		components.push_back(d);
	}

	bool combination::match(const std::string& text)
	{
		for (auto definition : components)
		{
			if (definition->match(text))
				return true;
		}
		return false;
	}

	character::character(char c) : value(c) {}

	bool character::match(const std::string& text)
	{
		if (text.size() != 1) return false;
		return text.at(0) == value;
	}

	keyword::keyword(std::string&& content) : value(content) {}

	bool keyword::match(const std::string& text)
	{
		return text.compare(value) == 0;
	}

	// Lexer classes

	file_lexer::file_lexer(const std::string& source_contents, lexical_definition* rules) : source(source_contents), rules(rules) {}

	file_lexer::file_lexer(std::string&& source_contents, lexical_definition* rules) : source(std::move(source_contents)), rules(rules) {}

	language_lexer::language_lexer(lexical_definition* rules) : rules(rules) {}

	file_lexer language_lexer::lex_file(const std::string& file_location)
	{
		std::ifstream in(file_location, std::ios::in | std::ios::binary);
		if (!in) throw std::exception("Could not open file");

		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();

		return file_lexer(std::move(contents), rules);
	}

	file_lexer language_lexer::lex_string(const std::string& source_contents)
	{
		return file_lexer(source_contents, rules);
	}
}
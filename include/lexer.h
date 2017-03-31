#pragma once
#include <string>
#include <vector>

namespace lexer
{
	// Classes used to define the language

	class lexical_definition
	{
	public:
		virtual bool match(const std::string& text) = 0;
	};

	class combination : public lexical_definition
	{
	public:
		void add_definition(lexical_definition*);

		bool match(const std::string& text) override;

	private:
		std::vector<lexical_definition*> components;
	};

	class character : public lexical_definition
	{
	public:
		character(char);

		bool match(const std::string& text) override;

	private:
		char value;
	};

	class keyword : public lexical_definition
	{
	public:
		keyword(std::string&&);

		bool match(const std::string& text) override;

	private:
		const std::string value;
	};

	// Classes used to parse language

	class file_lexer
	{
	public:
		file_lexer(const std::string& source_contents, lexical_definition* rules);
		file_lexer(std::string&& source_contents, lexical_definition* rules);

	private:
		const std::string& source;
		lexical_definition* rules;
	};

	class language_lexer
	{
	public:
		language_lexer(lexical_definition* rules);

		file_lexer lex_file(const std::string& file_location);
		file_lexer lex_string(const std::string& source_contents);

	private:
		lexical_definition* rules;
	};
}
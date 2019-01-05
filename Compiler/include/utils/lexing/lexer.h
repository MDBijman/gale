#pragma once
#include <variant>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <string_view>
#include <stdint.h>
#include <regex>
#include <unordered_map>
#include <assert.h>
#include "utils/memory/pipe.h"

namespace lexing
{

	enum class token_kind
	{
		EPSILON,
		END_OF_INPUT,
		ERROR,
		
		IDENTIFIER,
		WORD,
		NUMBER,
		RIGHT_ARROW,
		LEFT_BRACKET,
		RIGHT_BRACKET,
		LEFT_ANGLE_BRACKET,
		RIGHT_ANGLE_BRACKET,
		LEFT_CURLY_BRACKET,
		RIGHT_CURLY_BRACKET,
		LEFT_SQUARE_BRACKET,
		RIGHT_SQUARE_BRACKET,
		VERTICAL_LINE,
		PLUS,
		MINUS,
		MUL,
		DIV,
		COLON,
		DOT,
		EQUALS,
		TWO_EQUALS,
		PERCENTAGE,
		LTEQ,
		GTEQ,
		FAT_RIGHT_ARROW,
		BACKSLASH,
		AND,
		OR,
		NOT,
		ARRAY_ACCESS,
		COMMA,
		SEMICOLON,
		TYPE_KEYWORD,
		MATCH_KEYWORD,
		MODULE_KEYWORD,
		PUBLIC_KEYWORD,
		LET_KEYWORD,
		IMPORT_KEYWORD,
		WHILE_KEYWORD,
		TRUE_KEYWORD,
		FALSE_KEYWORD,
		IF_KEYWORD,
		ELSEIF_KEYWORD,
		ELSE_KEYWORD,
		REF_KEYWORD
	};

	/*
	* \brief
	*	Represents a token. Negative values are reserved for special meanings.
	*	-1 indicates an epsilon token. -2 indicates an end of input token. -3 a new line.
	*/
	using lexer_position = std::string::const_iterator;
	using lexer_range = std::pair<lexer_position, lexer_position>;

	struct token
	{
		token_kind value;
		std::string_view text;
	};

	class token_stream_writer
	{
		memory::pipe<std::vector<lexing::token>>& out;
		std::vector<lexing::token> curr;

	public:
		token_stream_writer(memory::pipe<std::vector<lexing::token>>& out) : out(out) {}

		void write(token t)
		{
			curr.push_back(t);
			if (curr.size() > 256) out.send(std::move(curr));
		}

		void flush()
		{
			out.send(std::move(curr));
		}
	};

	static void advance(lexer_range& range, int n = 1) 
	{
		range.first += n;
	}

	static bool match(lexer_position first, std::string_view kw)
	{
		for (auto it = kw.begin(); it != kw.end(); it++)
			if (*it != *(first++)) return false;

		return true;
	}

	static token_kind match(lexer_range& range) 
	{
		switch (*range.first)
		{
		case '\"':
			advance(range);
			while (*range.first != '\"') advance(range);
			advance(range);
			return token_kind::WORD;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			number:
			advance(range);
			while (*range.first >= '0' && *range.first <= '9') advance(range);
			return token_kind::NUMBER;
		case '-': // -> - number
			advance(range);
			if (*range.first >= '0' && *range.first <= '9') goto number;
			else if (*range.first == '>') { advance(range); return token_kind::RIGHT_ARROW; }
			else return token_kind::MINUS;
		case '=': // => = ==
			advance(range);
			if (*range.first == '>') { advance(range); return token_kind::FAT_RIGHT_ARROW; }
			else if (*range.first == '=') { advance(range); return token_kind::TWO_EQUALS; }
			else return token_kind::EQUALS;
		case '(': advance(range); return token_kind::LEFT_BRACKET;
		case ')': advance(range); return token_kind::RIGHT_BRACKET;
		case '{': advance(range); return token_kind::LEFT_CURLY_BRACKET;
		case '}': advance(range); return token_kind::RIGHT_CURLY_BRACKET;
		case '[': advance(range); return token_kind::LEFT_SQUARE_BRACKET;
		case ']': advance(range); return token_kind::RIGHT_SQUARE_BRACKET;
		case '<': // <= <
			advance(range);
			if (*range.first == '=') { advance(range); return token_kind::LTEQ; }
			else return token_kind::LEFT_ANGLE_BRACKET;
		case '>': // >= =
			advance(range);
			if (*range.first == '=') { advance(range); return token_kind::GTEQ; }
			else return token_kind::EQUALS;
		case '|': // || |
			advance(range);
			if (*range.first == '|') { advance(range); return token_kind::OR; }
			else return token_kind::VERTICAL_LINE;
		case '&': 
			advance(range);
			if (*range.first == '&') { advance(range); return token_kind::AND; }
			return token_kind::ERROR;
		case '!': 
			advance(range); 
			if (*range.first == '!') { advance(range); return token_kind::ARRAY_ACCESS; }
			return token_kind::NOT;
		case ',': advance(range); return token_kind::COMMA;
		case ';': advance(range); return token_kind::SEMICOLON;
		case '+': advance(range); return token_kind::PLUS;
		case '*': advance(range); return token_kind::MUL;
		case '/': advance(range); return token_kind::DIV;
		case ':': advance(range); return token_kind::COLON;
		case '.': advance(range); return token_kind::DOT;
		case '%': advance(range); return token_kind::PERCENTAGE;
		case '\\':advance(range); return token_kind::BACKSLASH;
		case 't': // type true
			if (match(range.first, "type")) { advance(range, 4); return token_kind::TYPE_KEYWORD; }
			else if (match(range.first, "true")) { advance(range, 4); return token_kind::TRUE_KEYWORD; }
			goto identifier;
		case 'm': // match module
			if (match(range.first, "match")) { advance(range, 5); return token_kind::MATCH_KEYWORD; }
			else if (match(range.first, "module")) { advance(range, 6); return token_kind::MODULE_KEYWORD; }
			goto identifier;
		case 'p': // public
			if (match(range.first, "public")) { advance(range, 6); return token_kind::PUBLIC_KEYWORD; }
			goto identifier;
		case 'r': // ref
			if (match(range.first, "ref")) { advance(range, 3); return token_kind::REF_KEYWORD; }
			goto identifier;
		case 'i': // import if
			if (match(range.first, "import")) { advance(range, 6); return token_kind::IMPORT_KEYWORD; }
			else if (match(range.first, "if")) { advance(range, 2); return token_kind::IF_KEYWORD; }
			goto identifier;
		case 'w': // while
			if (match(range.first, "while")) { advance(range, 5); return token_kind::WHILE_KEYWORD; }
			goto identifier;
		case 'f': // false
			if (match(range.first, "false")) { advance(range, 5); return token_kind::FALSE_KEYWORD; }
			goto identifier;
		case 'l': // false
			if (match(range.first, "let")) { advance(range, 3); return token_kind::LET_KEYWORD; }
			goto identifier;
		case 'e': // elseif else
			if (match(range.first, "elseif")) { advance(range, 6); return token_kind::ELSEIF_KEYWORD; }
			else if (match(range.first, "else")) { advance(range, 4); return token_kind::ELSE_KEYWORD; }
			goto identifier;
		identifier:
		default:
			if (!((*range.first >= 'a' && *range.first <= 'z')
				|| (*range.first >= 'A' && *range.first <= 'Z')
				|| (*range.first >= '0' && *range.first <= '9')
				|| (*range.first == '.'))) return token_kind::ERROR;
		case '_':
			advance(range);
			while ((*range.first >= 'a' && *range.first <= 'z')
				|| (*range.first >= 'A' && *range.first <= 'Z')
				|| (*range.first >= '0' && *range.first <= '9')
				|| (*range.first == '_')
				|| (*range.first == '.')) advance(range);
			return token_kind::IDENTIFIER;
			break;
		}
	}

	enum class error_code
	{
		UNRECOGNIZED_SYMBOL,
	};

	struct error
	{
		error(const error_code& t, const std::string& m) : type(t), message(m) {}

		error_code type;
		std::string message;
	};

	class lexer
	{
	public:
		// Takes a string (e.g. file contents) and returns a token vector or an error code
		std::variant<error, std::vector<lexing::token>> parse(const std::string& input_string) const
		{
			lexer_range range{ input_string.begin(), input_string.end() };
			std::vector<lexing::token> result;
			result.reserve(input_string.size());

			// Offset into program from beginning for error reporting
			uint32_t line_count = 1;
			uint32_t character_count = 0;

			while (range.first != range.second)
			{
				while ((*range.first == ' ') || (*range.first == '\n') || (*range.first == '\t') || (*range.first == '\r'))
				{
					if (*range.first == '\n')
					{
						line_count++;
						character_count = 0;
					}

					++character_count;
					++range.first;
					if (range.first == range.second)
					{
						result.push_back(token{ token_kind::END_OF_INPUT, "" });
						return result;
					}
				}

				lexer_position before_match = range.first;
				token_kind id = match(range);

				if (id == token_kind::EPSILON)
				{
					const auto error_message =
						std::string("Unrecognized symbol \'")
						.append(std::string(1, *range.first))
						.append("\' at line ")
						.append(std::to_string(line_count))
						.append(", offset ")
						.append(std::to_string(character_count));
					return error{ error_code::UNRECOGNIZED_SYMBOL, error_message };
				}

				const auto token_size = std::distance(before_match, range.first);
				assert(token_size > 0);

				// token_size is 64 bits signed but always positive so we can cast to uint32_t
				character_count += static_cast<uint32_t>(token_size);

				const std::string_view tokenized(&*before_match, token_size);
				result.push_back(token{ id,tokenized });
			}

			result.push_back(token{ token_kind::END_OF_INPUT, "" });
			return result;
		}
	};
}

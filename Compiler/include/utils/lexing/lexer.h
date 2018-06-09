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
	/*
	* \brief
	*	Represents a token. Negative values are reserved for special meanings.
	*	-1 indicates an epsilon token. -2 indicates an end of input token. -3 a new line.
	*/
	using token_id = int32_t;
	using lexer_position = std::string::const_iterator;
	using lexer_range = std::pair<lexer_position, lexer_position>;

	constexpr token_id epsilon = -1;
	constexpr token_id end_of_input = -2;
	constexpr token_id new_line = -3;

	struct token
	{
		token_id value;
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
			if (curr.size() > 64) out.send(std::move(curr));
		}

		void flush()
		{
			out.send(std::move(curr));
		}
	};

	struct rules
	{
		token_id match(lexer_range& range) const
		{
			// \".*?\"
			if (match_strings)
			{
				if (*range.first != '\"')
					goto match_numbers;

				for (auto i = range.first + 1; i != range.second; i++)
				{
					if (*i == '\"')
					{
						range.first = i + 1;
						return string_token;
					}
				}

				goto match_numbers;
			}

		match_numbers:
			// \\-?[1-9][0-9]*|0
			if (match_numbers)
			{
				auto i = range.first;
				if (*i == '0')
				{
					range.first = range.first + 1;
					return number_token;
				}

				if (*i == '-') i++;
				if (!isdigit(*i) || (*i == '0')) goto match_tokens;
				while (i != range.second && isdigit(*i)) i++;

				range.first = i;
				return number_token;
			}

		match_tokens:
			for (int i = 0; i < tokens.size(); i++)
			{
				auto& token = tokens.at(i);

				auto token_it = token.begin();
				if (static_cast<size_t>(range.second - range.first) < token.size()) continue;

				auto text_it = range.first;

				while (token_it != token.end())
					if (*token_it++ != *text_it++) goto skip_token;

				range.first = text_it;
				return token_ids.at(i);

			skip_token:;
			}

			// [a-zA-Z_][a-zA-Z0-9_.]*
			if (match_identifiers)
			{
				if (!isalpha(*range.first) && (*range.first != '_')) goto match_fail;
				auto i = range.first + 1;
				while (i != range.second)
				{
					if (!isalnum(*i) && !(*i == '_') && !(*i == '.'))
						break;
					else
						i++;
				}

				// If matched at least 1 char
				if (i != range.first)
				{
					range.first = i;
					return identifier_token;
				}
			}

		match_fail:
			return -1;
		}

		token_id create_token(std::string token_to_match)
		{
			tokens.push_back(std::move(token_to_match));
			token_ids.push_back(token_generator);
			return token_generator++;
		}

		token_id create_identifier_token()
		{
			assert(!match_identifiers);
			match_identifiers = true;
			identifier_token = token_generator;
			return token_generator++;
		}

		token_id create_number_token()
		{
			assert(!match_numbers);
			match_numbers = true;
			number_token = token_generator;
			return token_generator++;
		}

		token_id create_string_token()
		{
			assert(!match_strings);
			match_strings = true;
			string_token = token_generator;
			return token_generator++;
		}

		rules& operator=(const rules& other)
		{
			tokens = other.tokens;
			token_ids = other.token_ids;

			token_generator = other.token_generator;

			string_token = other.string_token;
			number_token = other.number_token;
			identifier_token = other.identifier_token;
			match_strings = other.match_strings;
			match_numbers = other.match_numbers;
			match_identifiers = other.match_identifiers;
			return *this;
		}

	private:
		token_id token_generator = 1;

		// special tokens
		token_id string_token, number_token, identifier_token;
		bool match_strings = false, match_numbers = false, match_identifiers = false;

		std::vector<std::string> tokens;
		std::vector<std::string> keywords;
		std::vector<token_id> token_ids;
	};

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
		explicit lexer(const rules& rules) : rules(rules) {}

		// Takes a string (e.g. file contents) and returns a token vector or an error code
		std::optional<error> parse(const std::string& input_string, token_stream_writer writer) const
		{
			lexer_range range{ input_string.begin(), input_string.end() };

			// Offset into program from beginning for error reporting
			uint32_t line_count = 1;
			uint32_t character_count = 0;

			while (range.first != range.second)
			{
				while (isspace(*range.first))
				{
					if (*range.first == '\n')
					{
						line_count++;
						character_count = 0;
					}

					character_count++;
					++range.first;
					if (range.first == range.second)
					{
						writer.write(token{ end_of_input, "" });
						writer.flush();
						return std::nullopt;
					}
				}

				const lexer_range range_copy{ range };
				const token_id id = rules.match(range);

				if (id == -1)
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

				const auto token_size = std::distance(range_copy.first, range.first);
				assert(token_size > 0);

				// token_size is 64 bits signed but always positive so we can cast to uint32_t
				character_count += static_cast<uint32_t>(token_size);

				const std::string_view tokenized(&*range_copy.first, token_size);
				writer.write(token{ id, tokenized });
			}

			writer.write(token{ end_of_input, "" });
			writer.flush();
			return std::nullopt;
		}

	private:
		rules rules;
	};
}

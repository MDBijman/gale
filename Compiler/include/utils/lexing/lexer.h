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

namespace utils
{
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
			std::string text;
		};

		struct rules
		{
			token_id match(lexer_range& range) const
			{
				for (int i = 0; i < tokens.size(); i++)
				{
					auto& token = tokens.at(i);

					auto token_it = token.begin();
					auto text_it = range.first;
					bool match = true;

					while(match && !(text_it == range.second) && !(token_it == token.end()))
					{
						if (*token_it != *text_it)
						{
							match = false;
							continue;
						}

						token_it++;
						text_it++;
					}

					if (token_it == token.end())
					{
						range.first = text_it;
						return token_ids.at(i);
					}
				}

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
					if (!isdigit(*i) || (*i == '0')) goto match_keywords;
					while (i != range.second && isdigit(*i)) i++;

					range.first = i;
					return number_token;
				}

			match_keywords:
				// [a-zA-Z_][a-zA-Z0-9_.]*
				if (match_keywords)
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
						return keyword_token;
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

			token_id create_keyword_token()
			{
				assert(!match_keywords);
				match_keywords = true;
				keyword_token = token_generator;
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
				keyword_token = other.keyword_token;
				match_strings = other.match_strings;
				match_numbers = other.match_numbers;
				match_keywords = other.match_keywords;
				return *this;
			}

		private:
			token_id token_generator = 1;

			// special tokens
			token_id string_token, number_token, keyword_token;
			bool match_strings = false, match_numbers = false, match_keywords = false;

			std::vector<std::string> tokens;
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
			std::variant<
				std::vector<token>,
				error
			> parse(const std::string& input_string) const
			{
				std::vector<token> result;
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
							result.push_back(token{ new_line, "" });
							character_count = 0;
						}

						character_count++;
						++range.first;
						if (range.first == range.second) return result;
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
					result.push_back(token{ id, std::string(tokenized) });
				}

				result.push_back(token{ end_of_input, "" });

				return result;
			}

		private:
			rules rules;
		};
	}
}

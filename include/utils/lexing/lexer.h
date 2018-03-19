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
		*	0 indicates an epsilon token. -1 indicates an end of input token. -2 a new line.
		*/
		using token_id = int32_t;
		using lexer_position = std::string::const_iterator;
		using lexer_range = std::pair<lexer_position, lexer_position>;

		constexpr token_id epsilon = 1;
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
				std::smatch match;
				if (std::regex_search(range.first, range.second, match, regex_object, std::regex_constants::match_continuous))
				{
					range = match.suffix();
					for (unsigned int i = 1; i < match.size(); i++)
						if (match[i].matched) return regex_group_to_token_id.at(i - 1);
				}
				return -1;
			}

			token_id create_token(std::string regex_rule)
			{
				token_definitions.insert({ token_generator, regex_rule });
				return token_generator++;
			}

			void compile()
			{
				regex_group_to_token_id.clear();

				std::string regex_string;
				int subgroup = 0;
				for (auto& rule : token_definitions)
				{
					regex_group_to_token_id.insert({ subgroup++, rule.first });
					regex_string.append("(").append(rule.second).append(")").append("|");
				}
				regex_string.pop_back();
				regex_object = std::regex{ regex_string.c_str() };
			}

			rules& operator=(const rules& other)
			{
				token_definitions = other.token_definitions;
				token_generator = other.token_generator;

				compile();
				return *this;
			}

		private:
			token_id token_generator = 1;

			std::unordered_map<size_t, token_id> regex_group_to_token_id;
			std::unordered_map<token_id, std::string> token_definitions;

			std::regex regex_object;
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

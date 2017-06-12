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

namespace tools
{
	namespace lexing
	{
		/*
		* \brief
		*	Represents a token. Negative values are reserved for special meanings.
		*	0 indicates an epsilon token. -1 indicates an end of input token.
		*/
		using token_id = int32_t;
		using lexer_position = std::string::const_iterator;
		using lexer_range = std::pair<lexer_position, lexer_position>;

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
					for (int i = 1; i < match.size(); i++)
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
			error(error_code t, std::string m) : type(t), message(m) {}

			error_code type;
			std::string message;
		};

		class lexer
		{
		public:
			lexer(const rules& rules) : rules(rules) {}

			// Takes a string (e.g. file contents) and returns a token vector or an error code
			std::variant<
				std::vector<token>,
				error
			> parse(const std::string& input_string)
			{
				std::vector<token> result;
				lexer_range range{ input_string.begin(), input_string.end() };

				while (range.first != range.second)
				{
					while (isspace(*range.first))
					{
						range.first++;
						if (range.first == range.second) return result;
					}

					auto range_copy{ range };
					auto id = rules.match(range);

					if (id == -1)
						return error{ error_code::UNRECOGNIZED_SYMBOL, "Unrecognized symbol starting with: " + *range.first };

					auto token_size = std::distance(range_copy.first, range.first);
					std::string_view tokenized(&*range_copy.first, token_size);
					result.push_back(token{ id, std::string(tokenized) });
				}

				return result;
			}

		private:
			rules rules;
		};
	}
}

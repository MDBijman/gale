#pragma once
#include <variant>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <string_view>
#include <stdint.h>
#include <regex>

namespace lexer
{
	/*
	* \brief 
	*	Represents a token. Negative values are reserved for special meanings.
	*	0 indicates an epsilon token. -1 indicates an end of input token.
	*/
	using token_id = int32_t;
	using lexer_position = std::string::const_iterator;
	using lexer_range = std::pair<lexer_position, lexer_position>;

	/*
	* \brief A pair of <token name, regex string>
	*/
	using token_definitions = std::vector<std::pair<std::string, std::string>>;

	struct rules
	{
		rules(const token_definitions& rules) : token_definitions(rules)
		{
			std::string regex_string;
			for (auto& rule : rules)
			{
				regex_string.append("(").append(rule.second).append(")").append("|");
			}
			regex_string.pop_back();
			regex_object = std::regex{regex_string.c_str()};
		}

		token_id match(lexer_range& range) const
		{
			std::smatch match;
			if (std::regex_search(range.first, range.second, match, regex_object, std::regex_constants::match_continuous ))
			{
				range = match.suffix();
				for (int i = 1; i < match.size(); i++)
					if (match[i].matched) return i - 1;
			}
			return -1;
		}

	private:
		const token_definitions token_definitions;

		std::regex regex_object;
	};

	enum class error_code
	{
		UNRECOGNIZED_SYMBOL,
	};

	class lexer
	{
	public:
		lexer(const rules& rules) : rules(rules) {}

		// Takes a string (e.g. file contents) and returns a token vector or an error code
		std::variant<
			std::vector<token_id>,
			error_code
		> parse(const std::string& input_string)
		{
			std::vector<token_id> result;
			lexer_range range{input_string.begin(), input_string.end()};

			while (range.first != range.second)
			{
				while (isspace(*range.first))
				{
					range.first++;
					if (range.first == range.second) return result;
				}

				auto id = rules.match(range);

				if (id == -1) return error_code::UNRECOGNIZED_SYMBOL;

				result.push_back(id);
			}

			return result;
		}

		const rules rules;
	};
}

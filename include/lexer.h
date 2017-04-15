#pragma once
#include <variant>
#include <unordered_map>
#include <vector>
#include <string>

namespace lexer
{
	using token_id = size_t;

	namespace detail
	{
		struct token
		{
			token_id id;
			std::string_view characters;

		};

		struct token_tree
		{
			token_tree(token t) : matches_or_token(t) {}

			void put(token t)
			{
				// Empty
				if (holds<std::monostate>())
				{
					matches_or_token = t;
				}
				// Leaf
				else if (holds<token>())
				{
					// Adding token as final
					if (t.characters.size() == 0 || get<token>().characters.size() == 0)
						throw std::runtime_error("Colliding token tree");

					// Adding multi character token
					auto old_token = get<token>();

					matches_or_token = token_map{};
					auto& new_map = get<token_map>();

					new_map.insert({ t.characters[0], token_tree{ t } });

					new_map.insert({ old_token.characters[0], token_tree{ old_token } });
				}
				// Internal
				else if (holds<token_map>())
				{
					if (t.characters.size() == 0)
						throw std::runtime_error("Colliding token tree");

					auto& current_map = get<token_map>();
					t.advance();
					current_map.insert({});
				}

				auto& matches = std::get<std::unordered_map<char, token_tree>>(matches_or_token);
				auto subgroup = matches.find(token.characters[0]);

				if (token.characters.size() == 1)
				{
					if (subgroup != matches.end())
						throw std::runtime_error("Incompatible token definitions");

					matches.insert({ token.characters[0], token_tree{token.id} });
					return;
				}

				
				subgroup = matches.insert({ token.characters[0], token_tree{} }).first;

				(*subgroup).second.put(&token[1]);
			}

			token_id get() const
			{
				return std::get<token_id>(matches_or_token);
			}

			token_id get(std::string_view::iterator& token) const
			{
				if (is_leaf())
					throw std::runtime_error("This lexer subgroup is a leaf");

				auto& children = std::get<std::unordered_map<char, token_tree>>(matches_or_token);
				auto it = children.find(token[0]);
				if (it == children.end())
					throw std::runtime_error("No subgroup corresponding to given char");

				if (it->second.is_leaf())
					return it->second.get();
				else
					return it->second.get(++token);
			}

		private:
			template<class T>
			bool holds() 
			{
				return std::holds_alternative<T>(matches_or_token);
			}

			template<class T>
			decltype(auto) get()
			{
				return std::get<T>(matches_or_token);
			}

			using token_map = std::unordered_map<char, token_tree>;
			std::variant<
				std::monostate,
				token_map,
				token
			> matches_or_token;
		};
	}

	struct lexer_rules
	{
		lexer_rules(const std::vector<std::string>& tokens) : possible_tokens(tokens)
		{
			for (decltype(auto) token : possible_tokens)
				root_matches.put(token);
		}

		token_id match(std::string_view::iterator& token) const
		{
			return root_matches.get(token);
		}

		std::string token_id_to_string(token_id id) const
		{
			return possible_tokens.at(id);
		}

	private:
		const std::vector<std::string> possible_tokens;
		detail::token_tree root_matches;
	};

	class lexer
	{
	public:
		lexer(const lexer_rules& rules) : rules(rules) {}

		// Takes a string (e.g. file contents) and returns a token vector
		std::vector<token_id> parse(const std::string_view& input_string)
		{
			std::vector<token_id> result;

			auto input_iterator = input_string.begin();
			while (input_iterator != input_string.end())
			{
				while (isspace(*input_iterator))
				{
					input_iterator++;
				}

				if (input_iterator == input_string.end())
					return result;

				auto id = rules.match(input_iterator);

				result.push_back(id);
			}
		}

		const lexer_rules rules;
	};
}

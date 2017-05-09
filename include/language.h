#pragma once
#include "parser.h"
#include "lexer.h"
#include <algorithm>
#include <functional>

namespace language
{
	using terminal = parsing::terminal;
	using non_terminal = parsing::non_terminal;
	using symbol = parsing::symbol;

	struct bnf_rule
	{
		bnf_rule(const std::string& lhs, const std::vector<std::string>& rhs) : lhs(lhs), rhs(rhs) {}

		const std::string lhs;
		const std::vector<std::string> rhs;
	};

	struct ebnf_rule
	{
		ebnf_rule(const std::string& lhs, const std::vector<std::variant<std::string, char>>& rhs) : lhs(lhs), rhs(rhs) {}

		bool contains_metatoken()
		{
			auto first_meta_token = std::find_if(rhs.begin(), rhs.end(), [](auto& segment) {
				return std::holds_alternative<char>(segment);
			});

			return first_meta_token != rhs.end();
		}

		auto find(char token)
		{
			return std::find_if(rhs.begin(), rhs.end(), [token](auto& segment) {
				return std::holds_alternative<char>(segment) && (std::get<char>(segment) == token);
			});
		}

		std::vector<bnf_rule> to_bnf(std::function<non_terminal(const std::string&)> nt_generator)
		{
			// Turns a nested vector into single vector
			auto flatten = [](std::vector<std::vector<bnf_rule>>& vec) {
				std::vector<bnf_rule> flat_simplified;
				for (auto& rules : vec)
				{
					for (auto& rule : rules)
					{
						flat_simplified.push_back(rule);
					}
				}
				return flat_simplified;
			};

			decltype(rhs)::const_iterator it;
			if ((it = find('|')) != rhs.end())
			{
				std::vector<ebnf_rule> split_alt = split_on('|');
				std::vector<std::vector<bnf_rule>> simplified;
				std::transform(split_alt.begin(), split_alt.end(), std::back_inserter(simplified), [&](auto& rule) {
					return rule.to_bnf(nt_generator);
				});
	
				return flatten(simplified);
			}
			else if ((it = find('(')) != rhs.end())
			{
				int stack = 1;
				auto left_bracket_it = it;
				// Find the matching bracket and replace the group with a new rule
				while (it++ != rhs.end())
				{
					if (std::holds_alternative<char>(*it))
					{
						auto current_char = std::get<char>(*it);
						if (current_char == ')')
						{
							stack--; 
							if (stack == 0)
								break;
						}
						else if(current_char == '(')
							stack++;
					}
				}

				// Non matching brackets
				if (stack != 0)
					throw std::runtime_error("Incomplete group found");

				// Matching bracket found
				auto begin_group = left_bracket_it + 1;
				auto end_group = it - 1;

				auto rule_name = std::string(lhs).append("_g");
				auto rule_rhs = std::vector<std::variant<std::string, char>>();
				std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

				ebnf_rule additional_rule{rule_name, rule_rhs};
				nt_generator(rule_name);

				// Remove group from new rule and replace with the new rule identifier
				auto modified_rhs{ rhs };
				auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
				auto right_bracket_offset = std::distance(rhs.begin(), it);
				modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
				modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_name);
				ebnf_rule modified_rule{ lhs, modified_rhs };

				std::vector<std::vector<bnf_rule>> simplified_rules;
				simplified_rules.push_back(additional_rule.to_bnf(nt_generator));
				simplified_rules.push_back(modified_rule.to_bnf(nt_generator));
				return flatten(simplified_rules);
			}
			else if ((it = find('[')) != rhs.end())
			{
				int stack = 1;
				auto left_bracket_it = it;
				// Find the matching bracket and replace the group with a new rule
				while (it++ != rhs.end())
				{
					if (std::holds_alternative<char>(*it))
					{
						auto current_char = std::get<char>(*it);
						if (current_char == ']')
						{
							stack--; 
							if (stack == 0)
								break;
						}
						else if(current_char == '[')
							stack++;
					}
				}

				// Non matching brackets
				if (stack != 0)
					throw std::runtime_error("Incomplete optional found");

				// Matching bracket found
				// Create two new rules, and replace [E] with X
				// X -> epsilon
				// X -> E
				auto begin_group = left_bracket_it + 1;
				auto end_group = it - 1;

				auto rule_name = std::string(lhs).append("_o").append(std::get<std::string>(*(left_bracket_it + 1)));
				auto rule_rhs = std::vector<std::variant<std::string, char>>();
				std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

				ebnf_rule additional_rule{ rule_name, rule_rhs };
				bnf_rule epsilon_rule{ rule_name, { "epsilon" } };
				nt_generator(rule_name);

				// Remove optional from new rule and replace with the new rule identifier
				auto modified_rhs{ rhs };
				auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
				auto right_bracket_offset = std::distance(rhs.begin(), it);
				modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
				modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_name);
				ebnf_rule modified_rule{ lhs, modified_rhs };

				std::vector<std::vector<bnf_rule>> simplified_rules;
				simplified_rules.push_back(additional_rule.to_bnf(nt_generator));
				simplified_rules.push_back(modified_rule.to_bnf(nt_generator));
				auto result_rules = flatten(simplified_rules);
				result_rules.push_back(epsilon_rule);
				return result_rules;
			}
			else if ((it = find('*')) != rhs.end())
			{
				it--;
				if (!std::holds_alternative<std::string>(*it))
					throw std::runtime_error("Can only multiply non terminals/terminal");

				auto multiplied_symbol = std::get<std::string>(*it);
				auto new_symbol_name = std::string(multiplied_symbol).append("_m");

				// Create two new rules, and replace E* with X
				// X -> epsilon
				// X -> E, X
				auto additional_rule = bnf_rule(new_symbol_name, { multiplied_symbol, new_symbol_name });
				auto epsilon_rule = bnf_rule(new_symbol_name, { "epsilon" });
				nt_generator(additional_rule.lhs);

				// Remove multiplier from new rule and replace the identifier with the new rule identifier
				auto multiplier_offset = std::distance(rhs.begin(), it) + 1;
				auto multiplied_offset = std::distance(rhs.begin(), it);
				auto modified_rhs{ rhs };
				modified_rhs.at(multiplied_offset) = additional_rule.lhs;
				modified_rhs.erase(modified_rhs.begin() + multiplier_offset);
				ebnf_rule modified_rule{ lhs, modified_rhs };

				std::vector<bnf_rule> simplified_rules;
				simplified_rules.push_back(additional_rule);
				simplified_rules.push_back(epsilon_rule);

				auto simplified_rest = modified_rule.to_bnf(nt_generator);
				for (auto& rule : simplified_rest)
				{
					simplified_rules.push_back(rule);
				}

				return simplified_rules;
			}
			else
			{
				std::vector<std::string> new_rhs;
				for (auto& segment : rhs)
				{
					new_rhs.push_back(std::get<std::string>(segment));
				}
				return { bnf_rule{ lhs, new_rhs } };
			}
		}

		const std::string lhs;
		const std::vector<std::variant<std::string, char>> rhs;

	private:
		std::vector<ebnf_rule> split_on(char token)
		{
			std::vector<ebnf_rule> rules;

			std::vector<std::variant<std::string, char>> new_rule;
			for (auto it = rhs.begin(); it != rhs.end(); ++it)
			{
				auto variant = *it;

				if (std::holds_alternative<char>(variant) && (std::get<char>(variant) == token))
				{
					rules.push_back(ebnf_rule{ lhs, new_rule });
					new_rule.clear();
				}
				else
				{
					new_rule.push_back(variant);
				}
			}
			rules.push_back(ebnf_rule{ lhs, new_rule });
			return rules;
		}
	};

	class language
	{
	public:
		language()
		{
			add_terminal("end_of_input", parsing::end_of_input);
			add_terminal("epsilon", parsing::epsilon);
		}

		lexing::lexer generate_lexer()
		{
			return lexing::lexer{ token_rules };
		}

		parsing::parser generate_parser()
		{
			return parsing::parser{ parsing::rules{ bnf_rules } };
		}

		language& define_terminal(const std::string& name, const std::string& rule)
		{
			terminal token = add_terminal(name);
			token_rules.insert({ token, rule });
			return *this;
		}

		language& define_non_terminal(const std::string& name)
		{
			add_non_terminal(name);
			return *this;
		}

		language& define_rule(ebnf_rule r)
		{
			auto bnf_converted_rules = r.to_bnf([&](const std::string& name) { return add_non_terminal(name); });
			for (auto rule : bnf_converted_rules)
			{
				translation.insert({ rule.lhs, r });
				add_rule(rule.lhs, rule.rhs);
			}
			return *this;
		}

		parsing::symbol to_symbol(const std::string& symbol_name) const
		{
			if (symbols.find(symbol_name) != symbols.end())
				return symbols.at(symbol_name);

			throw std::runtime_error("Unknown symbol");
		}

		std::string to_string(symbol symbol) const
		{
			for (auto pair : symbols)
			{
				if (pair.second == symbol)
					return pair.first;
			}

			throw std::runtime_error("Unknown symbol");
		}

	protected:
		terminal add_terminal(const std::string& name)
		{
			return add_terminal(name, generate_terminal());
		}

		terminal add_terminal(const std::string& name, terminal value)
		{
			if (symbols.find(name) != symbols.end())
				throw std::runtime_error("Terminal or non terminal already exists with name");

			symbols.insert({ name, value });
			return value;
		}

		non_terminal add_non_terminal(const std::string& name)
		{
			return add_non_terminal(name, generate_non_terminal());
		}

		non_terminal add_non_terminal(const std::string& name, non_terminal value)
		{
			if (symbols.find(name) != symbols.end())
				throw std::runtime_error("Terminal or non terminal already exists with name");

			symbols.insert({ name, value });
			return value;
		}

		void add_rule(const std::string& lhs_string, const std::vector<std::string>& rhs_strings)
		{
			if (symbols.find(lhs_string) == symbols.end())
				throw std::runtime_error("Non terminal does not exist");

			non_terminal lhs;
			{
				auto lhs_symbol = symbols.at(lhs_string);

				if (lhs_symbol.is_terminal())
					throw std::runtime_error("Cannot have a terminal lhs");

				lhs = lhs_symbol.get_non_terminal();
			}

			std::vector<symbol> rhs;

			for (decltype(auto) rhs_segment_string : rhs_strings)
			{
				if (symbols.find(rhs_segment_string) != symbols.end())
					rhs.push_back(symbols.at(rhs_segment_string));
				else
					throw std::runtime_error("Unknown identifier");
			}

			add_rule(lhs, std::move(rhs));
		}

		void add_rule(non_terminal nt, std::vector<symbol> rhs)
		{
			auto nt_rules = bnf_rules.equal_range(nt);
			for (auto rule = nt_rules.first; rule != nt_rules.second; ++rule)
			{
				//if (rhs == rule->second)
				//	throw std::runtime_error("Rule already exists");
			}

			bnf_rules.insert({ nt, rhs });
		}

		auto begin()
		{
			return bnf_rules.begin();
		}

		auto end()
		{
			return bnf_rules.end();
		}

	private:
		std::multimap<non_terminal, std::vector<symbol>> bnf_rules;
		std::unordered_map<std::string, ebnf_rule> translation;

		std::unordered_map<std::string, symbol> symbols;

		std::unordered_map<lexing::token_id, std::string> token_rules;

		terminal generate_terminal()
		{
			return t_generator++;
		}

		non_terminal generate_non_terminal()
		{
			return nt_generator++;
		}

		terminal t_generator = 1;
		non_terminal nt_generator = 1;
	};

}

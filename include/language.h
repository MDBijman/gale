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

	namespace ebnf {
		enum meta_char {
			alt,
			lsb,
			rsb,
			lrb,
			rrb,
			star
		};
	}

	using namespace ebnf;

	struct bnf_rule
	{
		bnf_rule(const non_terminal& lhs, const std::vector<symbol>& rhs) : lhs(lhs), rhs(rhs) {}

		const non_terminal lhs;
		const std::vector<symbol> rhs;
	};

	struct ebnf_rule
	{
		ebnf_rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs) : lhs(lhs), rhs(rhs) {}

		bool contains_metatoken() const
		{
			auto first_meta_token = std::find_if(rhs.begin(), rhs.end(), [](auto& segment) {
				return std::holds_alternative<meta_char>(segment);
			});

			return first_meta_token != rhs.end();
		}

		auto find(char token) const
		{
			return std::find_if(rhs.begin(), rhs.end(), [token](auto& segment) {
				return std::holds_alternative<meta_char>(segment) && (std::get<meta_char>(segment) == token);
			});
		}

		std::vector<bnf_rule> to_bnf(std::function<non_terminal()> nt_generator) const
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
			if ((it = find(alt)) != rhs.end())
			{
				std::vector<ebnf_rule> split_alt = split_on(alt);
				std::vector<std::vector<bnf_rule>> simplified;
				std::transform(split_alt.begin(), split_alt.end(), std::back_inserter(simplified), [&](auto& rule) {
					return rule.to_bnf(nt_generator);
				});
	
				return flatten(simplified);
			}
			else if ((it = find(lrb)) != rhs.end())
			{
				int stack = 1;
				auto left_bracket_it = it;
				// Find the matching bracket and replace the group with a new rule
				while (it++ != rhs.end())
				{
					if (std::holds_alternative<meta_char>(*it))
					{
						auto current_char = std::get<meta_char>(*it);
						if (current_char == rrb)
						{
							stack--; 
							if (stack == 0)
								break;
						}
						else if(current_char == lrb)
							stack++;
					}
				}

				// Non matching brackets
				if (stack != 0)
					throw std::runtime_error("Incomplete group found");

				// Matching bracket found
				auto begin_group = left_bracket_it + 1;
				auto end_group = it - 1;

				auto rule_lhs = nt_generator();
				auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
				std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

				ebnf_rule additional_rule{rule_lhs, rule_rhs};

				// Remove group from new rule and replace with the new rule identifier
				auto modified_rhs{ rhs };
				auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
				auto right_bracket_offset = std::distance(rhs.begin(), it);
				modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
				modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
				ebnf_rule modified_rule{ lhs, modified_rhs };

				std::vector<std::vector<bnf_rule>> simplified_rules;
				simplified_rules.push_back(additional_rule.to_bnf(nt_generator));
				simplified_rules.push_back(modified_rule.to_bnf(nt_generator));
				return flatten(simplified_rules);
			}
			else if ((it = find(lsb)) != rhs.end())
			{
				int stack = 1;
				auto left_bracket_it = it;
				// Find the matching bracket and replace the group with a new rule
				while (++it != rhs.end())
				{
					if (std::holds_alternative<meta_char>(*it))
					{
						auto current_char = std::get<meta_char>(*it);
						if (current_char == rsb)
						{
							stack--; 
							if (stack == 0)
								break;
						}
						else if(current_char == lsb)
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

				auto symbol_after_left_bracket = std::get<symbol>(*(left_bracket_it + 1));

				non_terminal rule_lhs = nt_generator();

				auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
				std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

				ebnf_rule additional_rule{ rule_lhs, rule_rhs };
				bnf_rule epsilon_rule{ rule_lhs, { parsing::epsilon } };

				// Remove optional from new rule and replace with the new rule identifier
				auto modified_rhs{ rhs };
				auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
				auto right_bracket_offset = std::distance(rhs.begin(), it);
				modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
				modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
				ebnf_rule modified_rule{ lhs, modified_rhs };

				std::vector<std::vector<bnf_rule>> simplified_rules;
				simplified_rules.push_back(additional_rule.to_bnf(nt_generator));
				simplified_rules.push_back(modified_rule.to_bnf(nt_generator));
				auto result_rules = flatten(simplified_rules);
				result_rules.push_back(epsilon_rule);
				return result_rules;
			}
			else if ((it = find(star)) != rhs.end())
			{
				it--;
				if (!std::holds_alternative<symbol>(*it))
					throw std::runtime_error("Can only multiply non terminals/terminal");

				auto multiplied_symbol = std::get<symbol>(*it);
				non_terminal new_symbol = nt_generator();

				// Create two new rules, and replace E* with X
				// X -> epsilon
				// X -> E, X
				auto additional_rule = bnf_rule(new_symbol, { multiplied_symbol, new_symbol });
				auto epsilon_rule = bnf_rule(new_symbol, { parsing::epsilon });

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
				std::vector<symbol> new_rhs;
				for (auto& segment : rhs)
				{
					new_rhs.push_back(std::get<symbol>(segment));
				}
				return { bnf_rule{ lhs, new_rhs } };
			}
		}

		const non_terminal lhs;
		const std::vector<std::variant<symbol, meta_char>> rhs;

	private:
		std::vector<ebnf_rule> split_on(meta_char token) const
		{
			std::vector<ebnf_rule> rules;

			std::vector<std::variant<symbol, meta_char>> new_rule;
			for (auto it = rhs.begin(); it != rhs.end(); ++it)
			{
				auto variant = *it;

				if (std::holds_alternative<meta_char>(variant) && (std::get<meta_char>(variant) == token))
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

	struct rule
	{
		ebnf_rule ebnf;
		std::vector<bnf_rule> bnf;
	};

	class language
	{
	public:
		language()
		{
			add_terminal("end_of_input", parsing::end_of_input);
			add_terminal("epsilon", parsing::epsilon);
		}

		std::vector<lexing::token_id> lex(const std::string& input_string) const
		{
			auto lexer = lexing::lexer{ token_rules };
			return std::get<std::vector<lexing::token_id>>(lexer.parse(input_string));
		}

		ast::node<symbol>* parse(non_terminal init, std::vector<terminal> input) const
		{
			auto parser_compatible_rules = std::multimap<non_terminal, std::vector<symbol>>();
			std::unordered_map<non_terminal, non_terminal> ebnf_ancestors;
			for (auto& rule : rules)
			{
				for (auto& bnf_rule : rule.bnf)
				{
					parser_compatible_rules.insert({ bnf_rule.lhs, bnf_rule.rhs });
					if (bnf_rule.lhs != rule.ebnf.lhs)
						ebnf_ancestors.insert({ bnf_rule.lhs, rule.ebnf.lhs });
				}
			}

			auto parser = parsing::parser{ parsing::rules{ parser_compatible_rules } };
			auto ast = parser.parse(init, input);

			std::function<void(ast::node<symbol>*)> bnf_to_ebnf = [&](ast::node<symbol>* node) {
				if (node->t.is_terminal())
					return;

				std::vector<ast::node<symbol>*> new_children;
				node->children.erase(std::remove_if(node->children.begin(), node->children.end(), [&](auto& child) {
					bnf_to_ebnf(child);

					if (!child->t.is_terminal() && !is_named(child->t.get_non_terminal()))
					{
						std::move(child->children.begin(), child->children.end(), std::back_inserter(new_children));
						child->children.clear();
						return true;
					}

					return false;
				}), node->children.end());

				std::move(new_children.begin(), new_children.end(), std::back_inserter(node->children));
			};

			bnf_to_ebnf(ast);

			return ast;
		}

		terminal create_terminal(const std::string& rule)
		{
			terminal token = generate_terminal();
			token_rules.insert({ token, rule });
			return token;
		}

		non_terminal create_non_terminal()
		{
			return generate_non_terminal();
		}

		terminal define_terminal(const std::string& name, const std::string& rule)
		{
			terminal token = add_terminal(name);
			token_rules.insert({ token, rule });
			return token;
		}

		non_terminal define_non_terminal(const std::string& name)
		{
			return add_non_terminal(name);
		}

		language& define_rule(ebnf_rule r)
		{
			auto bnf_converted_rules = r.to_bnf([&]() { return generate_non_terminal(); });
			rules.push_back(rule{ r, bnf_converted_rules });
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

		bool is_named(non_terminal nt) const
		{
			return std::find_if(symbols.begin(), symbols.end(), [nt](auto& x) { return (!x.second.is_terminal()) && (x.second.get_non_terminal() == nt); }) != symbols.end();
		}

	private:
		std::vector<rule> rules;

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

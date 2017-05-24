#pragma once
#include "bnf_parser.h"
#include <functional>
#include <set>

namespace language
{
	namespace ebnf
	{
		using terminal = bnf::terminal;
		using non_terminal = bnf::non_terminal;
		using symbol = bnf::symbol;

		namespace meta {
			enum meta_char {
				alt,
				lsb,
				rsb,
				lrb,
				rrb,
				star
			};
		}
		using namespace meta;

		using rule_stub = std::pair<non_terminal, std::vector<std::variant<symbol, meta_char>>>;

		struct rule
		{
			rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs, std::function<non_terminal(non_terminal)> nt_gen) : lhs(lhs), rhs(rhs), bnf(to_bnf(nt_gen)) {}

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

			const non_terminal lhs;
			const std::vector<std::variant<symbol, meta_char>> rhs;
			const std::vector<bnf::rule> bnf;

		private:
			std::vector<bnf::rule> to_bnf(std::function<non_terminal(non_terminal)> nt_generator) const
			{
				// Turns a nested vector into single vector
				auto flatten = [](std::vector<std::vector<bnf::rule>>& vec) {
					std::vector<bnf::rule> flat_simplified;
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
					std::vector<rule> split_alt = split_on(alt, nt_generator);
					std::vector<std::vector<bnf::rule>> simplified;
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
							else if (current_char == lrb)
								stack++;
						}
					}

					// Non matching brackets
					if (stack != 0)
						throw std::runtime_error("Incomplete group found");

					// Matching bracket found
					auto begin_group = left_bracket_it + 1;
					auto end_group = it - 1;

					auto rule_lhs = nt_generator(lhs);
					auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
					std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

					rule additional_rule{ rule_lhs, rule_rhs, nt_generator };

					// Remove group from new rule and replace with the new rule identifier
					auto modified_rhs{ rhs };
					auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
					auto right_bracket_offset = std::distance(rhs.begin(), it);
					modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
					modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
					rule modified_rule{ lhs, modified_rhs, nt_generator };

					std::vector<std::vector<bnf::rule>> simplified_rules;
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
							else if (current_char == lsb)
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

					non_terminal rule_lhs = nt_generator(lhs);

					auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
					std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

					rule additional_rule{ rule_lhs, rule_rhs, nt_generator };
					bnf::rule epsilon_rule{ rule_lhs, { bnf::epsilon } };

					// Remove optional from new rule and replace with the new rule identifier
					auto modified_rhs{ rhs };
					auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
					auto right_bracket_offset = std::distance(rhs.begin(), it);
					modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
					modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
					rule modified_rule{ lhs, modified_rhs, nt_generator };

					std::vector<std::vector<bnf::rule>> simplified_rules;
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
					non_terminal new_symbol = nt_generator(lhs);

					// Create two new rules, and replace E* with X
					// X -> epsilon
					// X -> E, X
					auto additional_rule = bnf::rule(new_symbol, { multiplied_symbol, new_symbol });
					auto epsilon_rule = bnf::rule(new_symbol, { bnf::epsilon });

					// Remove multiplier from new rule and replace the identifier with the new rule identifier
					auto multiplier_offset = std::distance(rhs.begin(), it) + 1;
					auto multiplied_offset = std::distance(rhs.begin(), it);
					auto modified_rhs{ rhs };
					modified_rhs.at(multiplied_offset) = additional_rule.lhs;
					modified_rhs.erase(modified_rhs.begin() + multiplier_offset);
					rule modified_rule{ lhs, modified_rhs, nt_generator };

					std::vector<bnf::rule> simplified_rules;
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
					return { bnf::rule{ lhs, new_rhs } };
				}
			}

			std::vector<rule> split_on(meta_char token, std::function<non_terminal(non_terminal)> nt_generator) const
			{
				std::vector<rule> rules;

				std::vector<std::variant<symbol, meta_char>> new_rule;
				for (auto it = rhs.begin(); it != rhs.end(); ++it)
				{
					auto variant = *it;

					if (std::holds_alternative<meta_char>(variant) && (std::get<meta_char>(variant) == token))
					{
						rules.push_back(rule{ lhs, new_rule, nt_generator });
						new_rule.clear();
					}
					else
					{
						new_rule.push_back(variant);
					}
				}
				rules.push_back(rule{ lhs, new_rule, nt_generator });
				return rules;
			}
		};

		class parser
		{
		public:
			parser() {}

			std::vector<lexing::token_id> lex(const std::string& input_string) const
			{
				auto lexer = lexing::lexer{ token_rules };
				auto output_or_error = lexer.parse(input_string);
				return std::get<std::vector<lexing::token_id>>(output_or_error);
			}

			ast::node<symbol>* parse(non_terminal init, std::vector<terminal> input) const
			{
				auto parser_compatible_rules = std::multimap<non_terminal, std::vector<symbol>>();
				for (auto& rule : rules)
				{
					for (auto& bnf_rule : rule.bnf)
					{
						parser_compatible_rules.insert({ bnf_rule.lhs, bnf_rule.rhs });
					}
				}

				auto parser = bnf::parser{ bnf::rules{ parser_compatible_rules } };
				auto ast = parser.parse(init, input);

				std::function<void(ast::node<symbol>*)> bnf_to_ebnf = [&](ast::node<symbol>* node) {
					if (node->t.is_terminal())
						return;

					std::vector<ast::node<symbol>*> new_children;
					node->children.erase(std::remove_if(node->children.begin(), node->children.end(), [&](auto& child) {
						bnf_to_ebnf(child);

						if (!child->t.is_terminal() && (ebnf_non_terminals.find(child->t.get_non_terminal()) == ebnf_non_terminals.end()))
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

			parser& create_rule(rule_stub r)
			{
				rules.push_back(rule{ r.first, r.second, [&](non_terminal p) { return generate_child_non_terminal(p); } });
				return *this;
			}

			const terminal end_of_input = bnf::end_of_input;
			const terminal epsilon = bnf::epsilon;

		private:
			std::vector<rule> rules;

			std::set<non_terminal> ebnf_non_terminals;

			std::unordered_map<lexing::token_id, std::string> token_rules;

			std::unordered_map<non_terminal, non_terminal> nt_child_parents;

			terminal generate_terminal()
			{
				return t_generator++;
			}

			non_terminal generate_non_terminal()
			{
				ebnf_non_terminals.insert(nt_generator);
				return nt_generator++;
			}

			non_terminal generate_child_non_terminal(non_terminal parent)
			{
				nt_child_parents.insert({ nt_generator, parent });
				return nt_generator++;
			}

			terminal t_generator = 1;
			non_terminal nt_generator = 1;
		};
	}
}
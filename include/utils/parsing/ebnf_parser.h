#pragma once
#include "bnf_parser.h"
#include "bnf_grammar.h"
#include <functional>
#include <memory>
#include <set>
#include <variant>

namespace utils
{
	namespace ebnf
	{
		using terminal = bnf::terminal;
		using non_terminal = bnf::non_terminal;
		using symbol = bnf::symbol;

		constexpr terminal end_of_input = bnf::end_of_input;
		constexpr terminal epsilon = bnf::epsilon;

		enum class error_code
		{
			BNF_ERROR,
		};

		struct error
		{
			error_code type;
			std::string message;
		};

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

		enum class child_type {
			REPETITION,
			GROUP,
			OPTIONAL
		};

		struct terminal_node
		{
			terminal_node(bnf::terminal_node* bnf_node) : value(bnf_node->value), token(bnf_node->token) {}

			std::string token;
			terminal value;
		};

		struct non_terminal_node
		{
			non_terminal_node(bnf::non_terminal_node* bnf_tree, std::unordered_map<non_terminal, std::pair<non_terminal, child_type>>& rule_inheritance) : value(bnf_tree->value)
			{
				for (auto& child : bnf_tree->children)
				{
					if (std::holds_alternative<bnf::non_terminal_node>(*child))
					{
						auto& child_node = std::get<bnf::non_terminal_node>(*child);
						children.push_back(std::make_unique<std::variant<terminal_node, non_terminal_node>>(non_terminal_node(&child_node, rule_inheritance)));
					}
					else
					{
						auto& child_node = std::get<bnf::terminal_node>(*child);
						children.push_back(std::make_unique<std::variant<terminal_node, non_terminal_node>>(terminal_node(&child_node)));
					}
				}


				decltype(children) new_children;

				auto it = children.begin();
				while (it != children.end())
				{
					auto child = it->get();

					if (std::holds_alternative<non_terminal_node>(*child) && (rule_inheritance.find(std::get<non_terminal_node>(*child).value) != rule_inheritance.end()))
					{
						auto& child_node = std::get<non_terminal_node>(*child);
						auto& inheritance_rule = rule_inheritance.at(child_node.value);

						switch (inheritance_rule.second)
						{
						case child_type::GROUP:
							std::move(child_node.children.begin(), child_node.children.end(), std::back_inserter(new_children));
							it = children.erase(it);
							break;
						case child_type::REPETITION:
						case child_type::OPTIONAL:
							// If repeated 0 times, skip
							if (child_node.children.size() == 1 && std::holds_alternative<terminal_node>(*child_node.children.at(0)) && (std::get<terminal_node>(*child_node.children.at(0)).value == epsilon))
							{
								it++;
							}
							else
							{
								std::move(child_node.children.begin(), child_node.children.end(), std::back_inserter(new_children));
								it = children.erase(it);
							}
							break;
						}
					}
					else
					{
						new_children.push_back(std::move(*it));
						it++;
					}
				}
				children = std::move(new_children);
			}

			std::vector<std::unique_ptr<std::variant<terminal_node, non_terminal_node>>> children;
			non_terminal value;
		};

		using node = std::variant<terminal_node, non_terminal_node>;

		struct rule
		{
			rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs) : lhs(lhs), rhs(rhs) {}

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

			// TODO simplify and DRY
			std::vector<bnf::rule> to_bnf(std::function<non_terminal(non_terminal, child_type)> nt_generator) const
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
					std::vector<rule> split_alt = split_on(alt);
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

					auto rule_lhs = nt_generator(lhs, child_type::GROUP);
					auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
					std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

					rule additional_rule{ rule_lhs, rule_rhs };

					// Remove group from new rule and replace with the new rule identifier
					auto modified_rhs{ rhs };
					auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
					auto right_bracket_offset = std::distance(rhs.begin(), it);
					modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
					modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
					rule modified_rule{ lhs, modified_rhs };

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

					non_terminal rule_lhs = nt_generator(lhs, child_type::OPTIONAL);

					auto rule_rhs = std::vector<std::variant<symbol, meta_char>>();
					std::copy(begin_group, end_group + 1, std::back_inserter(rule_rhs));

					rule additional_rule{ rule_lhs, rule_rhs };
					bnf::rule epsilon_rule{ rule_lhs, { bnf::epsilon } };

					// Remove optional from new rule and replace with the new rule identifier
					auto modified_rhs{ rhs };
					auto left_bracket_offset = std::distance(rhs.begin(), left_bracket_it);
					auto right_bracket_offset = std::distance(rhs.begin(), it);
					modified_rhs.erase(modified_rhs.begin() + left_bracket_offset, modified_rhs.begin() + right_bracket_offset + 1);
					modified_rhs.insert(modified_rhs.begin() + left_bracket_offset, rule_lhs);
					rule modified_rule{ lhs, modified_rhs };

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
					non_terminal new_symbol = nt_generator(lhs, child_type::REPETITION);

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
					rule modified_rule{ lhs, modified_rhs };

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

			const non_terminal lhs;
			const std::vector<std::variant<symbol, meta_char>> rhs;

		private:
			std::vector<rule> split_on(meta_char token) const
			{
				std::vector<rule> rules;

				std::vector<std::variant<symbol, meta_char>> new_rule;
				for (auto it = rhs.begin(); it != rhs.end(); ++it)
				{
					auto variant = *it;

					if (std::holds_alternative<meta_char>(variant) && (std::get<meta_char>(variant) == token))
					{
						rules.push_back(rule{ lhs, new_rule });
						new_rule.clear();
					}
					else
					{
						new_rule.push_back(variant);
					}
				}
				rules.push_back(rule{ lhs, new_rule });
				return rules;
			}
		};

		class parser
		{
		public:
			parser() {}

			std::variant<std::unique_ptr<node>, error> parse(non_terminal init, std::vector<bnf::terminal_node> input)
			{
				try {
					auto ast_or_error = bnf_parser.parse(init, input);

					// Handle error of bnf parser
					if (std::holds_alternative<bnf::error>(ast_or_error))
						return error{ error_code::BNF_ERROR, std::get<bnf::error>(ast_or_error).message };

					auto& extended_ast = std::get<std::unique_ptr<bnf::node>>(ast_or_error);

					// Convert from bnf to ebnf
					if (std::holds_alternative<bnf::terminal_node>(*extended_ast))
						return std::make_unique<node>(terminal_node{ &std::get<bnf::terminal_node>(*extended_ast) });
					else if (std::holds_alternative<bnf::non_terminal_node>(*extended_ast))
						return std::make_unique<node>(non_terminal_node{ &std::get<bnf::non_terminal_node>(*extended_ast), nt_child_parents });
					else
						return error{ error_code::BNF_ERROR, "BNF parser returned empty value" };
				}
				catch (lr::conflict e) {
					std::string error_message;

					switch (e.type)
					{
					case utils::lr::conflict::type::SHIFT_SHIFT:
						error_message.append("Shift/Shift conflict\n");
						break;
					case utils::lr::conflict::type::SHIFT_REDUCE:
						error_message.append("Shift/Reduce conflict\n");
						break;
					}

					error_message.append("Item set: ").append(std::to_string(e.item_set)).append("\n");
					error_message.append("Expected: ").append(e.expected).append("\n");

					if (auto it = nt_child_parents.find(e.rule.lhs); it != nt_child_parents.end())
					{
						auto rule = std::find_if(rules.begin(), rules.end(), [&](auto& rule) { return rule.lhs == it->first; });
						error_message.append("Rule: ").append(std::to_string(rule->lhs));
					}
					else
					{
						error_message.append("Rule: ").append(std::to_string(e.rule.lhs));
					}

					return error{ error_code::BNF_ERROR, error_message };
				}

			}

			parser& new_rule(rule r)
			{
				rules.push_back(r);
				auto bnf_rules = r.to_bnf([&](non_terminal p, child_type type) { return generate_child_non_terminal(p, type); });
				for (auto& bnf_rule : bnf_rules)
				{
					bnf_parser.new_rule({ bnf_rule.lhs, bnf_rule.rhs });
				}
				return *this;
			}

			terminal new_terminal()
			{
				return bnf_parser.new_terminal();
			}

			non_terminal new_non_terminal()
			{
				return bnf_parser.new_non_terminal();
			}

		private:
			bnf::parser bnf_parser;
			std::vector<rule> rules;

			std::unordered_map<non_terminal, std::pair<non_terminal, child_type>> nt_child_parents;

			non_terminal generate_child_non_terminal(non_terminal parent, child_type type)
			{
				auto nt = bnf_parser.new_non_terminal();
				nt_child_parents.insert({ nt, { parent, type } });
				return nt;
			}
		};
	}
}
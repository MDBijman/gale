#pragma once
#include "bnf_parser.h"
#include "bnf_grammar.h"
#include <functional>
#include <memory>
#include <set>
#include <variant>

namespace utils::ebnf
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

		static const char* meta_char_as_string[] = {
			"|",
			"[", "]",
			"(", ")",
			"*"
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
		const non_terminal lhs;
		const std::vector<std::variant<symbol, meta_char>> rhs;

		rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs) : lhs(lhs), rhs(rhs) {}

		bool contains_metatoken() const
		{
			auto first_meta_token = std::find_if(rhs.begin(), rhs.end(), [](auto& segment) {
				return std::holds_alternative<meta_char>(segment);
			});

			return first_meta_token != rhs.end();
		}

		std::vector<bnf::rule> to_bnf(std::function<non_terminal(non_terminal, child_type)> nt_generator) const
		{
			decltype(rhs)::const_iterator it = std::find_if(rhs.begin(), rhs.end(), [](auto& seg) {
				return std::holds_alternative<meta_char>(seg); });

			// No meta characters -> return rule as bnf::rule object
			if (it == rhs.end())
			{
				std::vector<symbol> new_rhs;
				std::transform(rhs.begin(), rhs.end(), std::back_inserter(new_rhs),
					[](auto& seg) { return std::get<symbol>(seg); });
				return { bnf::rule{ lhs, new_rhs } };
			}

			meta_char curr_meta = std::get<meta_char>(*it);
			switch (curr_meta)
			{
			case meta_char::star: return simplify_repetition(it, nt_generator); break;
			case meta_char::alt:  return simplify_alt(it, nt_generator);        break;
			case meta_char::lrb:  // Fallthrough
			case meta_char::lsb:  return simplify_group(it, nt_generator);      break;
			case meta_char::rrb:  // Fallthrough
			case meta_char::rsb:  // Fallthrough
			default: throw std::runtime_error("Error parsing rule");
			}
		}

	private:
		std::vector<bnf::rule> simplify_repetition(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const
		{
			// Create two new rules with lhs X, and replace E* with X
			// X -> epsilon
			// X -> E, X

			assert(it != rhs.begin());
			auto multiplied_symbol = std::get<symbol>(*(it - 1));

			non_terminal new_symbol = nt_generator(lhs, child_type::REPETITION);
			auto additional_rule = bnf::rule(new_symbol, { multiplied_symbol, new_symbol });
			auto epsilon_rule = bnf::rule(new_symbol, { bnf::epsilon });

			// Remove multiplier from new rule and replace the identifier with the new rule identifier
			std::vector<std::variant<symbol, meta_char>> modified_rhs;
			modified_rhs.insert(modified_rhs.begin(), rhs.begin(), it - 1);
			modified_rhs.push_back(new_symbol);
			modified_rhs.insert(modified_rhs.end(), it + 1, rhs.end());
			rule modified_rule{ lhs, modified_rhs };

			auto simplified_rules = modified_rule.to_bnf(nt_generator);
			simplified_rules.push_back(additional_rule);
			simplified_rules.push_back(epsilon_rule);

			return simplified_rules;
		}

		std::vector<bnf::rule> simplify_group(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const
		{
			// Create one new rule with lhs X, and replace [E] or (E) with X
			// X -> E
			// If [E] (i.e. optional group) also add:
			// X -> epsilon

			auto curr_meta = std::get<meta_char>(*it);
			bool is_optional_group = curr_meta == meta_char::lsb;
			// This is rrb ) if we are extracting a regular group, rsb ] if it is an optional group
			meta_char complement = is_optional_group ? meta_char::rsb : meta_char::rrb;

			int stack = 1;
			auto left_bracket_it = it;
			// Find the matching bracket
			while (it++ != rhs.end() && stack != 0)
			{
				if (!std::holds_alternative<meta_char>(*it))
					continue;

				auto curr_char = std::get<meta_char>(*it);
				if (curr_char == complement)
					stack--;
				else if (curr_char == curr_meta)
					stack++;
			}

			// Non matching brackets
			if (stack != 0) throw std::runtime_error("Incomplete group found");

			// Matching bracket found
			non_terminal group_lhs = nt_generator(lhs, is_optional_group ? child_type::OPTIONAL : child_type::GROUP);
			auto group_rhs = std::vector<std::variant<symbol, meta_char>>(left_bracket_it + 1, it - 1);
			rule additional_rule{ group_lhs, group_rhs };

			// Remove group from new rule and replace with the new rule identifier
			std::vector<std::variant<symbol, meta_char>> modified_rhs;
			modified_rhs.insert(modified_rhs.begin(), rhs.begin(), left_bracket_it);
			modified_rhs.push_back(group_lhs);
			modified_rhs.insert(modified_rhs.end(), it, rhs.end());
			rule modified_rule{ lhs, modified_rhs };

			// Concatenate vectors
			auto bnf_modified = modified_rule.to_bnf(nt_generator);
			auto bnf_added = additional_rule.to_bnf(nt_generator);
			std::move(bnf_added.begin(), bnf_added.end(), std::back_inserter(bnf_modified));

			// If we are extracting a [] group, also add epsilon rule 
			if (is_optional_group)
			{
				bnf::rule epsilon_rule{ group_lhs , { bnf::epsilon } };
				bnf_modified.push_back(epsilon_rule);
			}

			return bnf_modified;
		}

		std::vector<bnf::rule> simplify_alt(decltype(rhs)::const_iterator it,
			std::function<non_terminal(non_terminal, child_type)> nt_generator) const
		{
			rule left{ lhs, { rhs.begin(), it } };
			rule right{ lhs, { it + 1, rhs.end() } };
			auto bnf_left = left.to_bnf(nt_generator);
			auto bnf_right = right.to_bnf(nt_generator);
			// Combine simplified rules
			std::move(bnf_right.begin(), bnf_right.end(), std::back_inserter(bnf_left));
			return bnf_left;
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

				{
					auto& lhs = e.rule.first;
					auto& rhs = e.rule.second;

					switch (e.type)
					{
					case utils::lr::conflict::type::REDUCE_REDUCE:
						error_message.append("Reduce/Reduce conflict\n");
						break;
					case utils::lr::conflict::type::SHIFT_REDUCE:
						error_message.append("Shift/Reduce conflict\n");
						break;
					}

					error_message.append("Expected: ").append(e.expected).append("\n");

					if (auto it = nt_child_parents.find(lhs); it != nt_child_parents.end())
					{
						auto original_rule = std::find_if(rules.begin(), rules.end(), [&](auto& rule) {
							return rule.lhs == it->second.first;
						});
						error_message.append("Rule: ").append(std::to_string(original_rule->lhs)).append(" -> ");

						std::size_t i = 0;
						for (auto& elem : original_rule->rhs)
						{
							if (i == e.offset)
								error_message.append(" . ");
							i++;
							if (std::holds_alternative<meta::meta_char>(elem))
							{
								error_message.append(meta::meta_char_as_string[std::get<meta::meta_char>(elem)])
									.append(" ");
							}
							else
							{
								error_message.append(std::get<symbol>(elem)).append(" ");
							}
						}
					}
					else
					{
						error_message.append("Rule: ").append(std::to_string(lhs)).append(" -> ");
						for (auto& e : rhs)
							error_message.append(e).append(" ");
					}
					error_message.append("\n");
				}

				{
					auto& lhs = e.other.first;
					auto& rhs = e.other.second;

					if (auto it = nt_child_parents.find(lhs); it != nt_child_parents.end())
					{
						auto original_rule = std::find_if(rules.begin(), rules.end(), [&](auto& rule) {
							return rule.lhs == it->second.first;
						});
						error_message.append("Rule: ").append(std::to_string(original_rule->lhs)).append(" -> ");

						std::size_t i = 0;
						for (auto& elem : original_rule->rhs)
						{
							if (i == e.other_offset)
								error_message.append(" . ");
							i++;
							if (std::holds_alternative<meta::meta_char>(elem))
							{
								error_message.append(meta::meta_char_as_string[std::get<meta::meta_char>(elem)])
									.append(" ");
							}
							else
							{
								error_message.append(std::get<symbol>(elem)).append(" ");
							}
						}
					}
					else
					{
						error_message.append("Rule: ").append(std::to_string(lhs)).append(" -> ");
						std::size_t i = 0;
						for (auto& elem : rhs)
						{
							if (i == e.other_offset)
								error_message.append(" . ");
							i++;
							error_message.append(elem).append(" ");
						}
					}
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
				bnf_parser.new_rule({ bnf_rule.first, bnf_rule.second });
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
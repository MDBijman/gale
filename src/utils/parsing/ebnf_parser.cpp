#include "utils/parsing/ebnf_parser.h"
#include <stack>

namespace utils::ebnf
{
	// Terminal node

	terminal_node::terminal_node(bnf::terminal_node* bnf_node) : value(bnf_node->value), token(bnf_node->token) {}

	// Non terminal node

	non_terminal_node::non_terminal_node(non_terminal val,
		std::vector<std::unique_ptr<node>> children) : value(val), children(std::move(children))
	{}

	non_terminal_node bnf_to_ebnf(bnf::non_terminal_node& bnf_tree, 
		std::unordered_map<non_terminal, std::pair<non_terminal, child_type>>& rule_inheritance) 
	{
		// Post order traversal of the bnf tree to construct an ebnf tree.

		std::stack<std::unique_ptr<node>> converted;

		// Get bottom left child
		std::stack<std::variant<bnf::non_terminal_node*, bnf::terminal_node*>> s;
		s.push(&bnf_tree);

		std::variant<bnf::non_terminal_node*, bnf::terminal_node*> last_visited;

		while (!s.empty())
		{
			auto x = s.top();
			// If we have a non terminal node, we 'recursively' process it
			if (std::holds_alternative<bnf::non_terminal_node*>(x))
			{
				bnf::non_terminal_node* curr_node = std::get<bnf::non_terminal_node*>(x);

				// If the curr_node has no children, we just put it onto the converted stack
				if (curr_node->children.size() == 0)
				{
					// visit
					converted.push(std::make_unique<node>(non_terminal_node(curr_node->value, {})));
					last_visited = curr_node;
					s.pop();
					continue;
				}

				// If we have children, we check if we can find the last_visited node in the curr_node children
				// There are 3 cases: not found, found as the last child, found before the last child

				decltype(curr_node->children.begin()) it;
				for (it = curr_node->children.begin(); it != curr_node->children.end(); it++)
				{
					auto& child = *(it->get());
					// Compare curr vs child
					if ((std::holds_alternative<bnf::non_terminal_node>(child)
						&& std::holds_alternative<bnf::non_terminal_node*>(last_visited)
						&& (&std::get<bnf::non_terminal_node>(child) == std::get<bnf::non_terminal_node*>(last_visited)))
					|| (std::holds_alternative<bnf::terminal_node>(child)
						&& std::holds_alternative<bnf::terminal_node*>(last_visited)
						&& (&std::get<bnf::terminal_node>(child) == std::get<bnf::terminal_node*>(last_visited))))
					{
						break;
					}
				}

				// Case 1
				// last_visited is not part of the curr_node children, so we push the leftmost child onto the stack
				if (it == curr_node->children.end())
				{
					// Push left
					auto next = curr_node->children.begin()->get();
					if (std::holds_alternative<bnf::non_terminal_node>(*next))
						s.push(&std::get<bnf::non_terminal_node>(*next));
					else if (std::holds_alternative<bnf::terminal_node>(*next))
						s.push(&std::get<bnf::terminal_node>(*next));
				}
				// Case 2 
				// last_visited is the last of the curr_node children, so we have visited all children
				// We construct an ebnf non_terminal from the already converted children that are on the converted stack
				// They are transformed if they are a a result of a repetition/optional/group rule
				else if (it == curr_node->children.end() - 1)
				{
					s.pop();

					std::vector<std::unique_ptr<node>> reversed_children;
					for (int i = 0; i < curr_node->children.size(); i++)
					{
						auto child = std::move(converted.top());
						converted.pop();

						if (std::holds_alternative<non_terminal_node>(*child)
							&& rule_inheritance.find(std::get<non_terminal_node>(*child).value) != rule_inheritance.end())
						{
							auto& nt_child = std::get<non_terminal_node>(*child);
							auto& inheritance_rule = rule_inheritance.at(nt_child.value);

							// Actual bnf to ebnf conversion happens here
							// Repetitions '*' and optionals '[]' are lifted if they contain more than just an epsilon 
							// Groups '()' always get lifted up
							switch (inheritance_rule.second)
							{
							case child_type::REPETITION:
							case child_type::OPTIONAL:
								// If repeated 0 times, skip
								if (nt_child.children.size() == 1
									&& std::holds_alternative<terminal_node>(*nt_child.children.at(0))
									&& (std::get<terminal_node>(*nt_child.children.at(0)).value == epsilon))
								{
									continue;
								}
								// Fallthrough
							case child_type::GROUP:
								std::move(nt_child.children.rbegin(), nt_child.children.rend(), 
									std::back_inserter(reversed_children));
							}
						}
						else
						{
							reversed_children.push_back(std::move(child));
						}
					}

					std::vector<std::unique_ptr<node>> children;
					for (auto it = reversed_children.rbegin(); it != reversed_children.rend(); it++)
						children.push_back(std::move(*it));

					converted.push(std::make_unique<node>(non_terminal_node(curr_node->value, std::move(children))));
					last_visited = curr_node;
				}
				// Case 3 
				// last_visited is an element of the curr_node children, but not the last
				// Push its sibling onto the stack
				// In the next iteration(s) this node is further processed
				else
				{
					auto next = (it + 1)->get();
					if (std::holds_alternative<bnf::non_terminal_node>(*next))
						s.push(&std::get<bnf::non_terminal_node>(*next));
					else if (std::holds_alternative<bnf::terminal_node>(*next))
						s.push(&std::get<bnf::terminal_node>(*next));
				}
			}
			// If we have a terminal node, we just put it onto the converted stack
			else
			{
				last_visited = std::get<bnf::terminal_node*>(x);
				s.pop();
				converted.push(std::make_unique<node>(terminal_node(std::get<bnf::terminal_node*>(last_visited))));
			}
		}

		return std::move(std::get<non_terminal_node>(*converted.top()));
	}

	// Rule

	rule::rule(non_terminal lhs, const std::vector<std::variant<symbol, meta_char>>& rhs) : lhs(lhs), rhs(rhs) {}

	bool rule::contains_metatoken() const
	{
		auto first_meta_token = std::find_if(rhs.begin(), rhs.end(), [](auto& segment) {
			return std::holds_alternative<meta_char>(segment);
		});

		return first_meta_token != rhs.end();
	}

	std::vector<bnf::rule> rule::to_bnf(std::function<non_terminal(non_terminal, child_type)> nt_generator) const
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

	std::vector<bnf::rule> rule::simplify_repetition(decltype(rhs)::const_iterator it,
		std::function<non_terminal(non_terminal, child_type)> nt_generator) const
	{
		assert(std::holds_alternative<meta_char>(*it));
		assert(std::get<meta_char>(*it) == meta_char::star);

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

	std::vector<bnf::rule> rule::simplify_group(decltype(rhs)::const_iterator it,
		std::function<non_terminal(non_terminal, child_type)> nt_generator) const
	{
		assert(std::holds_alternative<meta_char>(*it));
		assert(std::get<meta_char>(*it) == meta_char::lrb || std::get<meta_char>(*it) == meta_char::lsb);

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

	std::vector<bnf::rule> rule::simplify_alt(decltype(rhs)::const_iterator it,
		std::function<non_terminal(non_terminal, child_type)> nt_generator) const
	{
		assert(std::holds_alternative<meta_char>(*it));
		assert(std::get<meta_char>(*it) == meta_char::alt);

		rule left{ lhs, { rhs.begin(), it } };
		rule right{ lhs, { it + 1, rhs.end() } };
		auto bnf_left = left.to_bnf(nt_generator);
		auto bnf_right = right.to_bnf(nt_generator);
		// Combine simplified rules
		std::move(bnf_right.begin(), bnf_right.end(), std::back_inserter(bnf_left));
		return bnf_left;
	}

	// Parser

	std::variant<std::unique_ptr<node>, error> parser::parse(non_terminal init, std::vector<bnf::terminal_node> input)
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
				return std::make_unique<node>(bnf_to_ebnf(std::get<bnf::non_terminal_node>(*extended_ast), nt_child_parents));
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
				case utils::lr::conflict::type::REDUCE_REDUCE: error_message.append("Reduce/Reduce conflict\n"); break;
				case utils::lr::conflict::type::SHIFT_REDUCE:  error_message.append("Shift/Reduce conflict\n");  break;
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

	parser& parser::new_rule(rule r)
	{
		rules.push_back(r);
		auto bnf_rules = r.to_bnf([&](non_terminal p, child_type type) { return generate_child_non_terminal(p, type); });
		for (auto& bnf_rule : bnf_rules)
		{
			bnf_parser.new_rule({ bnf_rule.first, bnf_rule.second });
		}
		return *this;
	}

	terminal parser::new_terminal()
	{
		return bnf_parser.new_terminal();
	}

	non_terminal parser::new_non_terminal()
	{
		return bnf_parser.new_non_terminal();
	}

	non_terminal parser::generate_child_non_terminal(non_terminal parent, child_type type)
	{
		auto nt = bnf_parser.new_non_terminal();
		nt_child_parents.insert({ nt, { parent, type } });
		return nt;
	}
}
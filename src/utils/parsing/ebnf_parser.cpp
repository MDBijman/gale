#include "utils/parsing/ebnf_parser.h"
#include "utils/algorithms/tree_traversal.h"
#include <stack>

namespace utils::ebnf
{
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

	void parser::generate(non_terminal init)
	{
		this->bnf_parser.generate(init);
	}

	std::variant<bnf::tree, error> parser::parse(std::vector<bnf::terminal_node> input)
	{
		try {
			auto bnf_results = bnf_parser.parse(input);

			// Handle error of bnf parser
			if (std::holds_alternative<bnf::error>(bnf_results))
				return error{ error_code::BNF_ERROR, std::get<bnf::error>(bnf_results).message };

			return convert(std::move(std::get<bnf::tree>(bnf_results)));
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

	bnf::tree parser::convert(bnf::tree in)
	{
		auto order = utils::algorithms::breadth_first(in);

		// First calculate sizes and allocate proper sizes. 
		std::vector<size_t> future_parents;
		std::vector<bool> disappears;
		future_parents.resize(order.size());
		disappears.resize(order.size());
		for (auto id : order)
		{
			bnf::node& n = in.get_node(id);

			if (n.kind == bnf::node_type::NON_TERMINAL)
			{
				auto& children = in.get_children_of(id);

				disappears[id] =
					(nt_child_parents.find(in.get_non_terminal(n.value_id).first) != nt_child_parents.end());

				for (auto child : children)
				{
					if (disappears[id]) future_parents[child] = future_parents[id];
					else future_parents[child] = id;
				}

				children.clear();
			}
		}

		for (auto id : order)
		{
			if (id == in.root()) continue;

			bnf::node& n = in.get_node(id);

			if (disappears[id]) in.free(id);
			else in.get_children_of(future_parents[id]).push_back(id);
		}

		return in;
	}
}
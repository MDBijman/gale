#include "utils/parsing/ebnfe_parser.h"
#include "utils/algorithms/tree_traversal.h"

namespace utils::ebnfe
{
	parser::parser() {}

	void parser::generate(non_terminal init)
	{
		ebnf_parser.generate(init);
	}

	std::variant<bnf::tree, error> parser::parse(std::vector<bnf::terminal_node> input)
	{
		auto ebnf_results = ebnf_parser.parse(input);
		if (std::holds_alternative<ebnf::error>(ebnf_results))
			return error{ error_code::EBNF_PARSER_ERROR, std::get<ebnf::error>(ebnf_results).message };

		return convert(std::move(std::get<bnf::tree>(ebnf_results)));
	}

	parser& parser::new_transformation(std::variant<terminal, non_terminal> s, transformation_type type)
	{

			transformation_rules.insert({ s, type });
			return *this;
	}

	parser& parser::new_rule(rule r)
	{
		ebnf_parser.new_rule(r);
		return *this;
	}

	terminal parser::new_terminal()
	{
		return ebnf_parser.new_terminal();
	}

	non_terminal parser::new_non_terminal()
	{
		return ebnf_parser.new_non_terminal();
	}

	bnf::tree parser::convert(bnf::tree in)
	{
		auto order = utils::algorithms::post_order(in);

		std::vector<bnf::node_id> converted;
		for (auto id : order)
		{
			auto& node = in.get_node(id);

			if (node.kind == bnf::node_type::NON_TERMINAL)
			{
				auto& children = in.get_children_of(id);

				std::vector<bnf::node_id> new_children;
				for (int i = children.size(); i > 0; i--)
				{
					auto child = converted.at(converted.size() - i);
					auto& child_node = in.get_node(child);

					if (child_node.kind == bnf::node_type::NON_TERMINAL)
					{
						auto& child_data = in.get_non_terminal(child_node.value_id);
						auto it = transformation_rules.find(child_data.first);
						if (it == transformation_rules.end())
						{
							new_children.push_back(child);
							continue;
						}
						auto rule = it->second;

						if (rule == transformation_type::REPLACE_IF_ONE_CHILD)
						{
							if (child_data.second.size() == 1) rule = transformation_type::REPLACE_WITH_CHILDREN;
							else rule = transformation_type::KEEP;
						}

						switch (rule)
						{
						case transformation_type::REMOVE_IF_ONE_CHILD:
							if (child_data.second.size() == 1)
							{
								in.free(child);
								break;
							}
							new_children.push_back(child);
							break;
						case transformation_type::REPLACE_WITH_CHILDREN:
							in.free(child);
							std::move(child_data.second.begin(), child_data.second.end(), std::back_inserter(new_children));
							break;
						case transformation_type::REMOVE:
							in.free(child);
							break;
						case transformation_type::KEEP:
						default:
							new_children.push_back(child);
						}
					}
					else
					{
						auto& child_data = in.get_terminal(child_node.value_id);

						auto it = transformation_rules.find(child_data.first);
						if (it == transformation_rules.end())
						{
							new_children.push_back(child);
							continue;
						}

						switch (it->second)
						{
						case transformation_type::REMOVE:
							in.free(child);
							continue;
						case transformation_type::KEEP:
						default:
							new_children.push_back(child);
						}
					}
				}

				converted.resize(converted.size() - children.size());
				children = std::move(new_children);
			}

			converted.push_back(id);
		}

		return in;
	}
}
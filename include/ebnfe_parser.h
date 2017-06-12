#pragma once
#include "ebnf_parser.h"
#include <functional>
#include <memory>
#include <vector>
#include <variant>

namespace tools
{
	namespace ebnfe
	{
		using terminal = ebnf::terminal;
		using non_terminal = ebnf::non_terminal;
		using symbol = ebnf::symbol;
		using rule = ebnf::rule;

		enum class error_code
		{
			EBNF_PARSER_ERROR,
			OTHER
		};

		struct error
		{
			error_code type;
			std::string message;
		};

		enum class transformation_type
		{
			REMOVE,
			REPLACE_WITH_CHILDREN,
			KEEP,
			REMOVE_IF_ONE_CHILD
		};

		struct terminal_node
		{
			terminal_node(ebnf::terminal_node* ebnf_tree) : value(ebnf_tree->value), token(ebnf_tree->token)
			{
			}

			std::string token;
			terminal value;
		};

		struct non_terminal_node
		{
			non_terminal_node(ebnf::non_terminal_node* ebnf_tree, std::function<transformation_type(std::variant<terminal, non_terminal>)> transformation_rules) : value(ebnf_tree->value)
			{
				for (auto& child : ebnf_tree->children)
				{
					if (std::holds_alternative<ebnf::non_terminal_node>(*child))
					{
						auto& child_node = std::get<ebnf::non_terminal_node>(*child);
						children.push_back(std::move(std::make_unique<std::variant<terminal_node, non_terminal_node>>(non_terminal_node(&child_node, transformation_rules))));
					}
					else
					{
						auto& child_node = std::get<ebnf::terminal_node>(*child);
						children.push_back(std::move(std::make_unique<std::variant<terminal_node, non_terminal_node>>(terminal_node(&child_node))));
					}
				}

				decltype(children) new_children;

				auto it = children.begin();
				while (it != children.end())
				{
					auto child = it->get();

					if (std::holds_alternative<terminal_node>(*child))
					{
						auto t_child = &std::get<terminal_node>(*child);
						auto rule = transformation_rules(t_child->value);

						switch (rule)
						{
							// Remove node
						case transformation_type::REMOVE:
							it++;
							break;
							// Keep node
						case transformation_type::KEEP:
						default:
							new_children.push_back(std::move(*it));
							it++;
							break;
						}
					}
					else
					{
						auto nt_child = &std::get<non_terminal_node>(*child);
						auto rule = transformation_rules(nt_child->value);

						switch (rule)
						{
							// Remove if there is a single child
						case transformation_type::REMOVE_IF_ONE_CHILD:
							if (nt_child->children.size() != 1)
							{
								new_children.push_back(std::move(*it));
								it++;
								break;
							}
							// Remove node and children
						case transformation_type::REMOVE:
							it = children.erase(it);
							break;
							// Remove node and promote children
						case transformation_type::REPLACE_WITH_CHILDREN:
							std::move(nt_child->children.begin(), nt_child->children.end(), std::back_inserter(new_children));
							it = children.erase(it);
							break;
							// Keep node
						case transformation_type::KEEP:
						default:
							new_children.push_back(std::move(*it));
							it++;
							break;
						}
					}
				}
				children = std::move(new_children);
			}

			std::vector<std::unique_ptr<std::variant<terminal_node, non_terminal_node>>> children;
			non_terminal value;
		};

		using node = std::variant<terminal_node, non_terminal_node>;

		class parser
		{
		public:
			parser() {}

			std::variant<std::unique_ptr<node>, error> parse(non_terminal init, std::vector<bnf::terminal_node> input)
			{
				auto ebnf_results = ebnf_parser.parse(init, input);
				if (std::holds_alternative<ebnf::error>(ebnf_results))
					return error{ error_code::EBNF_PARSER_ERROR, std::get<ebnf::error>(ebnf_results).message };

				auto ast = 
					std::get<ebnf::non_terminal_node>(
						std::move(*std::get<std::unique_ptr<ebnf::node>>(ebnf_results))
					);

				auto ebnfe_ast = std::make_unique<node>(non_terminal_node(&ast, [&](std::variant<terminal, non_terminal> s) {
					return transformation_rules.find(s) == transformation_rules.end() ?
						transformation_type::KEEP :
						transformation_rules.at(s);
				}));

				return ebnfe_ast;
			}

			parser& new_transformation(std::variant<terminal, non_terminal> s, transformation_type type)
			{
				transformation_rules.insert({ s, type });
				return *this;
			}

			parser& new_rule(rule r)
			{
				ebnf_parser.new_rule(r);
				return *this;
			}

			terminal new_terminal()
			{
				return ebnf_parser.new_terminal();
			}

			non_terminal new_non_terminal()
			{
				return ebnf_parser.new_non_terminal();
			}

		private:
			ebnf::parser ebnf_parser;

			std::unordered_map<std::variant<terminal, non_terminal>, transformation_type> transformation_rules;
		};
	}
}
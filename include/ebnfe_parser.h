#pragma once
#include "ebnf_parser.h"
#include <functional>

namespace language
{
	namespace ebnfe
	{
		using terminal = ebnf::terminal;
		using non_terminal = ebnf::non_terminal;
		using symbol = ebnf::symbol;
		using rule = ebnf::rule;

		enum class transformation_type
		{
			REMOVE,
			REPLACE_WITH_CHILDREN,
			KEEP,
			REMOVE_IF_ONE_CHILD
		};

		class node
		{
		public:
			node(ebnf::node* ebnf_tree, std::function<transformation_type(std::variant<terminal, non_terminal>)> transformation_rules) : value(ebnf_tree->value)
			{
				for (auto child : ebnf_tree->children)
					children.push_back(new node(child, transformation_rules));

				// Transform to ebnf
				std::vector<node*> new_children;

				auto it = children.begin();
				while (it != children.end())
				{
					auto child = *it;

					auto rule = transformation_rules(child->value.value);

					switch (rule)
					{
					case transformation_type::REMOVE_IF_ONE_CHILD:
						if (child->children.size() != 1)
						{
							new_children.push_back(*it);
							it++;
							break;
						}
						// Fallthrough
					case transformation_type::REMOVE:
					case transformation_type::REPLACE_WITH_CHILDREN:
						std::move(child->children.begin(), child->children.end(), std::back_inserter(new_children));
						it = children.erase(it);
						break;
					case transformation_type::KEEP:
					default:
						new_children.push_back(*it);
						it++;
						break;
					}
				}

				children = new_children;
			}

			~node()
			{
				for (auto child : children)
					delete child;
			}

			std::vector<node*> children;
			symbol value;
		};

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

		class parser
		{
		public:
			parser() {}

			std::variant<node*, error> parse(non_terminal init, std::vector<terminal> input)
			{
				auto ebnf_results = ebnf_parser.parse(init, input);
				if (std::holds_alternative<ebnf::error>(ebnf_results))
					return error{ error_code::EBNF_PARSER_ERROR, std::get<ebnf::error>(ebnf_results).message };

				auto ast = std::get<ebnf::node*>(ebnf_results);
				auto ebnfe_ast = new node(ast, [&](std::variant<terminal, non_terminal> s) {
					return transformation_rules.find(s) == transformation_rules.end() ?
						transformation_type::KEEP :
						transformation_rules.at(s);
				});

				delete ast;

				return ebnfe_ast;
			}

			terminal new_terminal()
			{
				return ebnf_parser.new_terminal();
			}

			non_terminal new_non_terminal()
			{
				return ebnf_parser.new_non_terminal();
			}

			parser& new_rule(rule r)
			{
				ebnf_parser.create_rule(r);
				return *this;
			}

			parser& new_transformation(std::variant<terminal, non_terminal> s, transformation_type type)
			{
				transformation_rules.insert({ s, type });
				return *this;
			}

		private:
			ebnf::parser ebnf_parser;

			std::unordered_map<std::variant<terminal, non_terminal>, transformation_type> transformation_rules;
		};
	}
}
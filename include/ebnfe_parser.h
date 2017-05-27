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

				std::function<void(int, node*)> print_ast = [&](int indentation, node* node) {
					for (int i = 0; i < indentation; i++)
						std::cout << "\t";
					if (node->value.is_terminal())
						std::cout << node->value.get_terminal() << std::endl;
					else
					{
						std::cout << node->value.get_non_terminal() << std::endl;
						for (auto child : node->children)
							print_ast(indentation + 1, child);
					}
				};
				print_ast(0, ebnfe_ast);
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

			parser& create_rule(ebnf::rule_stub r)
			{
				ebnf_parser.create_rule(r);
				return *this;
			}

			parser& add_transformation(std::variant<terminal, non_terminal> s, transformation_type type)
			{
				transformation_rules.insert({ s, type });
				return *this;
			}

			non_terminal new_non_terminal(std::string name)
			{
				auto nt = new_non_terminal();
				non_terminal_names.insert({ nt, name });
				return nt;
			}

			terminal new_terminal(std::string name)
			{
				auto t = new_terminal();
				terminal_names.insert({ t, name });
				return t;
			}

			bool has_symbol_name(std::variant<non_terminal, terminal> symbol)
			{
				if (std::holds_alternative<non_terminal>(symbol))
					return non_terminal_names.find(std::get<non_terminal>(symbol)) != non_terminal_names.end();
				else
					return terminal_names.find(std::get<terminal>(symbol)) != terminal_names.end();
			}

			const std::string& get_symbol_name(std::variant<non_terminal, terminal> symbol)
			{
				if (std::holds_alternative<non_terminal>(symbol))
					return non_terminal_names.at(std::get<non_terminal>(symbol));
				else
					return terminal_names.at(std::get<terminal>(symbol));
			}

		private:
			ebnf::parser ebnf_parser;

			std::unordered_map<non_terminal, std::string> non_terminal_names;
			std::unordered_map<terminal, std::string> terminal_names;

			std::unordered_map<std::variant<terminal, non_terminal>, transformation_type> transformation_rules;
		};
	}
}
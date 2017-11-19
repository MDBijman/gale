#pragma once
#include "pipeline.h"
#include "error.h"
#include "fe/language_definition.h"
#include "fe/data/extended_ast.h"

#include <string_view>
#include <assert.h>

namespace fe
{
	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<utils::ebnfe::node>, extended_ast::unique_node, cst_to_ast_error>
	{
	public:
		std::variant<extended_ast::unique_node, cst_to_ast_error> convert(std::unique_ptr<utils::ebnfe::node> node)
		{
			try
			{
				return conv(std::move(node));
			}
			catch (cst_to_ast_error e)
			{
				return e;
			}
		}

		extended_ast::unique_node conv(std::unique_ptr<utils::ebnfe::node> node)
		{
			using namespace extended_ast;

			if (std::holds_alternative<utils::ebnfe::non_terminal_node>(*node))
			{
				auto n = std::move(std::get<utils::ebnfe::non_terminal_node>(*node));
				auto node_type = n.value;

				// Convert children

				std::vector<unique_node> children;
				for (auto&& node_child : n.children)
				{
					children.push_back(conv(std::move(node_child)));
				}

				// Convert parent
			
				if (node_type == non_terminals::file)
					return std::make_unique<tuple>(std::move(children));

				else if (node_type == non_terminals::module_declaration)
					return std::make_unique<module_declaration>(std::move(children));

				else if (node_type == non_terminals::assignment)
					return std::make_unique<assignment>(std::move(children));

				else if (node_type == non_terminals::function_call)
					return std::make_unique<function_call>(std::move(children));

				else if (node_type == non_terminals::block)
					return std::make_unique<block>(std::move(children));

				else if (node_type == non_terminals::value_tuple)
					return std::make_unique<tuple>(std::move(children));

				else if (node_type == non_terminals::type_definition)
					return std::make_unique<type_definition>(std::move(children));

				else if (node_type == non_terminals::export_stmt)
					return std::make_unique<export_stmt>(std::move(children));

				else if (node_type == non_terminals::type_tuple)
					return std::make_unique<type_tuple>(std::move(children));

				else if (node_type == non_terminals::function_type)
					return std::make_unique<function_type>(std::move(children));

				else if (node_type == non_terminals::type_atom)
					return std::make_unique<type_atom>(std::move(children));

				else if (node_type == non_terminals::function)
					return std::make_unique<function>(std::move(children));

				else if (node_type == non_terminals::match)
					return std::make_unique<match>(std::move(children));

				else if (node_type == non_terminals::match_branch)
					return std::make_unique<match_branch>(std::move(children));

				else if (node_type == non_terminals::atom_variable_declaration)
					return std::make_unique<atom_declaration>(std::move(children));

				else if (node_type == non_terminals::tuple_variable_declaration)
					return std::make_unique<tuple_declaration>(std::move(children));

				else if (node_type == non_terminals::reference_type)
					return std::make_unique<reference_type>(std::move(children));

				else if (node_type == non_terminals::array_type)
					return std::make_unique<array_type>(std::move(children));

				else if (node_type == non_terminals::reference)
					return std::make_unique<reference>(std::move(children));

				else if (node_type == non_terminals::array_value)
					return std::make_unique<array_value>(std::move(children));

				else if (node_type == non_terminals::addition)
					return std::make_unique<addition>(std::move(children));

				else if (node_type == non_terminals::subtraction)
					return std::make_unique<subtraction>(std::move(children));

				else if (node_type == non_terminals::multiplication)
					return std::make_unique<multiplication>(std::move(children));

				else if (node_type == non_terminals::division)
					return std::make_unique<division>(std::move(children));

				else if (node_type == non_terminals::index)
					return std::make_unique<array_index>(std::move(children));

				else if (node_type == non_terminals::module_declaration)
					return std::make_unique<module_declaration>(std::move(children));

				else if (node_type == non_terminals::module_imports)
					return std::make_unique<import_declaration>(std::move(children));

				else if (node_type == non_terminals::while_loop)
					return std::make_unique<while_loop>(std::move(children));

				else if (node_type == non_terminals::equality)
					return std::make_unique<equality>(std::move(children));

				else if (node_type == non_terminals::identifier_tuple)
					return std::make_unique<identifier_tuple>(std::move(children));

				else
					throw cst_to_ast_error{
						std::string("Unknown CST non terminal node: ")
							.append(std::to_string(node_type))
				};
			}
			else if (std::holds_alternative<utils::ebnfe::terminal_node>(*node))
			{
				auto n = std::move(std::get<utils::ebnfe::terminal_node>(*node));
				auto node_type = n.value;
				// Do false and true
				if (node_type == terminals::number)
				{
					return std::make_unique<integer>(atoi(n.token.c_str()));
				}
				else if (node_type == terminals::word)
				{
					return std::make_unique<string>(n.token.substr(1, n.token.size() - 2));
				}
				else if (node_type == terminals::identifier)
				{
					auto split_on = [](std::string identifier, char split_on) -> std::vector<std::string> {
						std::vector<std::string> split_identifier;

						std::string::iterator begin_word = identifier.begin();
						for (auto it = identifier.begin(); it != identifier.end(); it++)
						{
							if (*it == split_on)
							{
								// Read infix
								split_identifier.push_back(std::string(begin_word, it));
								begin_word = it + 1;
								continue;
							}
							else
							{
								if (it == identifier.end() - 1)
									split_identifier.push_back(std::string(begin_word, it + 1));
							}
						}
						return split_identifier;
					};

					auto splitted = split_on(n.token, '.');

					return std::make_unique<identifier>(std::move(splitted));
				}
				else
				{
					throw cst_to_ast_error{
						std::string("Unknown CST terminal node: ")
							.append(std::to_string(node_type))
					};
				}
			}

			throw cst_to_ast_error{
				std::string("Empty/invalid CST node")
			};
		}

	private:
		bool holds_error(const std::variant<extended_ast::unique_node, cst_to_ast_error>& node)
		{
			return std::holds_alternative<cst_to_ast_error>(node);
		}

		cst_to_ast_error get_error(std::variant<extended_ast::unique_node, cst_to_ast_error>& node)
		{
			return std::get<cst_to_ast_error>(node);
		}

		extended_ast::unique_node& get_node(std::variant<extended_ast::unique_node, cst_to_ast_error>& node)
		{
			return std::get<extended_ast::unique_node>(node);
		}
	};
}
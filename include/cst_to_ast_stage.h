#pragma once
#include "pipeline.h"
#include "language_definition.h"
#include "error.h"
#include "tags.h"
#include "extended_ast.h"
#include <string_view>
#include <assert.h>

namespace fe
{
	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<tools::ebnfe::node>, extended_ast::unique_node, cst_to_ast_error>
	{
	public:
		std::variant<extended_ast::unique_node, cst_to_ast_error> convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			using namespace extended_ast;

			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto n = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));
				auto node_type = n.value;

				if (node_type == non_terminals::file)
				{
					std::vector<unique_node> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						auto converted_child = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child)) return get_error(converted_child);

						children.push_back(std::move(get_node(converted_child)));
					}
					return std::make_unique<tuple>(std::move(children));
				}
				else if (node_type == non_terminals::module_declaration)
				{
					// Module declaration has children [identifier]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& id = get_node(converted_identifier);
					
					return std::make_unique<module_declaration>(
						std::move(*dynamic_cast<identifier*>(id.get()))
					);
				}
				else if (node_type == non_terminals::assignment)
				{
					// Assignment has children [identifier value]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& identifier_node = get_node(converted_identifier);
					auto id = dynamic_cast<extended_ast::identifier*>(identifier_node.get());

					// Convert the value
					auto converted_value = convert(std::move(n.children.at(1)));
					if (holds_error(converted_value)) return get_error(converted_value);
					auto& value = get_node(converted_value);

					// Set function name
					if (auto fun = dynamic_cast<function*>(value.get()))
					{
						fun->name = identifier(*id);
					}

					return std::make_unique<assignment>(
						std::move(*id),
						std::move(value)
					);
				}
				else if (node_type == non_terminals::expression)
				{
					// Expression has children [tuple | number | word | identifier [tuple], expression* | ref expression]
					assert(n.children.size() >= 1);

					if (n.children.size() == 1)
					{
						return convert(std::move(n.children.at(0)));
					}
					else if (n.children.size() == 2)
					{
						// Function call

						auto id = convert(std::move(n.children.at(0)));
						if (holds_error(id)) return get_error(id);

						auto value = convert(std::move(n.children.at(1)));
						if (holds_error(value)) return get_error(value);

						return std::make_unique<function_call>(
							std::move(*dynamic_cast<extended_ast::identifier*>(get_node(std::move(id)).get())),
							unique_node(get_node(std::move(value))->copy())
						);
					}
					else
					{
						std::vector<unique_node> ast_children;

						for (decltype(auto) child : n.children)
						{
							auto new_child_or_error = convert(std::move(child));
							if (holds_error(new_child_or_error)) return get_error(new_child_or_error);

							ast_children.push_back(std::move(get_node(new_child_or_error)));
						}

						return std::make_unique<block>(
							std::move(ast_children)
						);
					}
				}
				else if (node_type == non_terminals::value_tuple)
				{
					// Tuple has children [values*]

					std::vector<unique_node> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						auto converted_child = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child)) return get_error(converted_child);

						children.push_back(std::move(get_node(converted_child)));
					}
					return std::make_unique<tuple>(std::move(children));
				}
				else if (node_type == non_terminals::type_definition)
				{
					// Type definition has children [identifier variable_declaration]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& id = get_node(converted_identifier);

					// Convert the variable_declaration
					auto converted_variable_declaration = convert(std::move(n.children.at(1)));
					if (holds_error(converted_variable_declaration)) return get_error(converted_variable_declaration);
					auto& variable_declaration = get_node(converted_variable_declaration);

					return std::make_unique<type_declaration>(
						std::move(*dynamic_cast<identifier*>(id.get())),
						std::move(*dynamic_cast<tuple_declaration*>(variable_declaration.get()))
					);
				}
				else if (node_type == non_terminals::export_stmt)
				{
					// Export has children [identifier*]

					std::vector<identifier> children;

					for (decltype(auto) child : n.children)
					{
						// Convert the identifier
						auto converted_identifier = convert(std::move(child));
						if (holds_error(converted_identifier)) return get_error(converted_identifier);
						auto& id = get_node(converted_identifier);

						children.push_back(std::move(*dynamic_cast<identifier*>(id.get())));
					}

					return std::make_unique<export_stmt>(
						std::move(children)
					);
				}
				else if (node_type == non_terminals::variable_declaration)
				{
					// Heap reference

					// Variable Declaration has children [([ref] type_expression identifier)*] 
					std::vector<std::variant<extended_ast::atom_declaration, extended_ast::function_declaration>> elements;

					int i = 0;
					while (i < n.children.size())
					{
						bool is_reference = std::holds_alternative<tools::ebnfe::terminal_node>(*n.children.at(i))
							&& (std::get<tools::ebnfe::terminal_node>(*n.children.at(i)).value == terminals::ref_keyword);

						if (is_reference) i++;

						auto converted_child_or_error = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child_or_error)) return get_error(converted_child_or_error);
						auto& converted_child = std::get<extended_ast::unique_node>(converted_child_or_error);

						auto converted_id_or_error = convert(std::move(n.children.at(i + 1)));
						if (holds_error(converted_id_or_error)) return get_error(converted_id_or_error);
						auto& converted_id = std::get<extended_ast::unique_node>(converted_id_or_error);

						if (auto atom = dynamic_cast<atom_type*>(converted_child.get()))
						{
							auto child = atom_declaration{
								std::move(*atom),
								std::move(*dynamic_cast<identifier*>(converted_id.get()))
							};

							if(is_reference) child.tags.set(tags::ref);

							elements.push_back(child);
						}
						else if (auto function = dynamic_cast<function_type*>(converted_child.get()))
						{
							auto child = function_declaration{
								std::move(*function),
								std::move(*static_cast<identifier*>(converted_id.get()))
							};

							if (is_reference) child.tags.set(tags::ref);

							elements.push_back(child);
						}
						else
						{
							return cst_to_ast_error{ "Variable declaration can only contain atom types and function types" };
						}

						i += 2;
					}

					return std::make_unique<tuple_declaration>(
						std::move(elements)
					);
				}
				else if (node_type == non_terminals::type_tuple)
				{
					// Type tuple has children [type_expression*]
					std::vector<std::variant<extended_ast::atom_type, extended_ast::function_type>> elements;

					for (decltype(auto) child : n.children)
					{
						auto converted_child_or_error = convert(std::move(child));
						if (holds_error(converted_child_or_error)) return get_error(converted_child_or_error);
						auto& converted_child = std::get<extended_ast::unique_node>(converted_child_or_error);

						if (auto atom = dynamic_cast<atom_type*>(converted_child.get()))
						{
							elements.push_back(std::move(*atom));
						}
						else if (auto function = dynamic_cast<function_type*>(converted_child.get()))
						{
							elements.push_back(std::move(*function));
						}
						else
						{
							return cst_to_ast_error{ "Type tuple can only contain atom and function declarations" };
						}
					}

					return std::make_unique<tuple_type>(
						std::move(elements)
					);
				}
				else if (node_type == non_terminals::type_function)
				{
					// Type Function has children [type_expression, type_expression]
					auto converted_from = convert(std::move(n.children.at(0)));
					if (holds_error(converted_from)) return get_error(converted_from);
					auto& type_from_node = get_node(converted_from);

					auto converted_to = convert(std::move(n.children.at(1)));
					if (holds_error(converted_to)) return get_error(converted_to);
					auto& type_to_node = get_node(converted_to);

					return std::make_unique<function_type>(
						std::move(*static_cast<tuple_type*>(type_from_node.get())),
						std::move(*static_cast<tuple_type*>(type_to_node.get()))
					);
				}
				else if (node_type == non_terminals::type_expression)
				{
					assert(n.children.size() >= 1 && n.children.size() <= 3);

					// Identifier
					if (std::holds_alternative<tools::ebnfe::terminal_node>(*n.children.at(0)))
					{
						assert(n.children.size() == 1 || n.children.size() == 3);

						auto converted_first = convert(std::move(n.children.at(0)));
						if (holds_error(converted_first)) return get_error(converted_first);
						auto& first = get_node(converted_first);

						if (n.children.size() == 3)
						{
							//std::visit(extended_ast::add_tag, first)(tags::array);
						}

						return std::make_unique<atom_type>(
							std::move(*dynamic_cast<identifier*>(first.get()))
						);
					}
					// Tuple or function type
					else
					{
						assert(n.children.size() == 1);

						return convert(std::move(n.children.at(0)));
					}
				}
				else if (node_type == non_terminals::function)
				{
					auto from = convert(std::move(n.children.at(0)));
					if (holds_error(from)) return get_error(from);

					auto to = convert(std::move(n.children.at(1)));
					if (holds_error(to)) return get_error(to);

					auto body = convert(std::move(n.children.at(2)));
					if (holds_error(body)) return get_error(body);

					return std::make_unique<function>(
						std::optional<identifier>(),
						std::move(*dynamic_cast<tuple_declaration*>(get_node(from).get())),
						std::move(get_node(to)),
						std::move(get_node(body))
					);
				}
				else if (node_type == non_terminals::branch)
				{
					// Branch has children [ [, branch_elements*, ] ]

					std::vector<conditional_branch_path> branches;
					for(auto i = n.children.begin() + 1; i != n.children.end() - 1; i++)
					{
						auto converted_branch = convert(std::move(*i));
						if (holds_error(converted_branch)) return get_error(converted_branch);

						branches.push_back(std::move(*dynamic_cast<conditional_branch_path*>(get_node(converted_branch).get())));
					}
					return std::make_unique<conditional_branch>(std::move(branches));
				}
				else if (node_type == non_terminals::branch_element)
				{
					auto test_path = convert(std::move(n.children.at(0)));
					if (holds_error(test_path)) return get_error(test_path);

					auto code_path = convert(std::move(n.children.at(1)));
					if (holds_error(code_path)) return get_error(code_path);

					return std::make_unique<conditional_branch_path>(
						std::move(get_node(test_path)),
						std::move(get_node(code_path))
					);
				}
				else
				{
					return cst_to_ast_error{
						std::string("Unknown CST non terminal node: ")
							.append(std::to_string(node_type))
					};
				}
				}
				else if (std::holds_alternative<tools::ebnfe::terminal_node>(*node))
				{
					auto n = std::move(std::get<tools::ebnfe::terminal_node>(*node));
					auto node_type = n.value;

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
						return cst_to_ast_error{
							std::string("Unknown CST terminal node: ")
								.append(std::to_string(node_type))
						};
					}
				}

				return cst_to_ast_error{
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
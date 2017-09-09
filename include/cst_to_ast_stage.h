#pragma once
#include "pipeline.h"
#include "language_definition.h"
#include "error.h"

namespace fe
{
	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<tools::ebnfe::node>, extended_ast::node, cst_to_ast_error>
	{
	public:
		std::variant<extended_ast::node, cst_to_ast_error> convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto n = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));
				auto node_type = n.value;

				if (node_type == file)
				{
					std::vector<extended_ast::node> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						auto converted_child = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child)) return get_error(converted_child);

						children.push_back(std::move(get_node(converted_child)));
					}
					return extended_ast::tuple(std::move(children));
				}
				else if (node_type == module_declaration)
				{
					// Module declaration has children [identifier]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& identifier = get_node(converted_identifier);

					return extended_ast::module_declaration(
						std::move(std::get<extended_ast::identifier>(identifier))
					);
				}
				else if (node_type == assignment)
				{
					// Assignment has children [identifier value]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& identifier_node = get_node(converted_identifier);
					auto& identifier = std::get<extended_ast::identifier>(identifier_node);

					// Convert the value
					auto converted_value = convert(std::move(n.children.at(1)));
					if (holds_error(converted_value)) return get_error(converted_value);
					auto& value = get_node(converted_value);

					// Set function name
					if (std::holds_alternative<extended_ast::function>(value))
					{
						std::get<extended_ast::function>(value).name = std::make_optional(extended_ast::identifier(identifier));
					}

					return extended_ast::assignment(
						std::move(identifier),
						std::move(extended_ast::make_unique(std::move(value)))
					);
				}
				else if (node_type == expression)
				{
					// Expression has children [tuple | number | word | identifier [tuple],  expression*]

					if(n.children.size() == 1)
					{
						return convert(std::move(n.children.at(0)));
					}
					else if (n.children.size() == 2)
					{
						auto id = convert(std::move(n.children.at(0)));
						if (holds_error(id)) return get_error(id);

						auto value = convert(std::move(n.children.at(1)));
						if (holds_error(value)) return get_error(value);

						return extended_ast::function_call{
							std::move(std::get<extended_ast::identifier>(get_node(std::move(id)))),
							extended_ast::make_unique(get_node(std::move(value)))
						};
					}
					else
					{
						std::vector<extended_ast::node> ast_children;

						for (decltype(auto) child : n.children)
						{
							auto new_child_or_error = convert(std::move(child));
							if (holds_error(new_child_or_error)) return get_error(new_child_or_error);

							ast_children.push_back(std::move(get_node(new_child_or_error)));
						}

						return extended_ast::block{
							std::move(ast_children)
						};
					}
				}
				else if (node_type == value_tuple)
				{
					// Tuple has children [values*]

					std::vector<extended_ast::node> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						auto converted_child = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child)) return get_error(converted_child);

						children.push_back(std::move(get_node(converted_child)));
					}
					return extended_ast::tuple(std::move(children));
				}
				else if (node_type == type_definition)
				{
					// Type definition has children [identifier variable_declaration]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));
					if (holds_error(converted_identifier)) return get_error(converted_identifier);
					auto& identifier = get_node(converted_identifier);

					// Convert the variable_declaration
					auto converted_variable_declaration = convert(std::move(n.children.at(1)));
					if (holds_error(converted_variable_declaration)) return get_error(converted_variable_declaration);
					auto& variable_declaration = get_node(converted_variable_declaration);

					return extended_ast::type_declaration(
						std::move(std::get<extended_ast::identifier>(identifier)),
						std::move(std::get<extended_ast::tuple_declaration>(variable_declaration))
					);
				}
				else if (node_type == export_stmt)
				{
					// Export has children [identifier*]

					std::vector<extended_ast::identifier> children;

					for (decltype(auto) child : n.children)
					{
						// Convert the identifier
						auto converted_identifier = convert(std::move(child));
						if (holds_error(converted_identifier)) return get_error(converted_identifier);
						auto& identifier = get_node(converted_identifier);

						children.push_back(std::move(std::get<extended_ast::identifier>(identifier)));
					}

					return extended_ast::export_stmt(
						std::move(children)
					);
				}
				else if (node_type == variable_declaration)
				{
					// Variable Declaration has children [(type_expression identifier)*] 
					std::vector<std::variant<extended_ast::atom_declaration, extended_ast::function_declaration>> elements;

					for (auto i = 0; i < n.children.size(); i += 2)
					{
						auto converted_child_or_error = convert(std::move(n.children.at(i)));
						if (holds_error(converted_child_or_error)) return get_error(converted_child_or_error);
						auto& converted_child = std::get<extended_ast::node>(converted_child_or_error);

						auto converted_id_or_error = convert(std::move(n.children.at(i + 1)));
						if (holds_error(converted_id_or_error)) return get_error(converted_id_or_error);
						auto& converted_id = std::get<extended_ast::node>(converted_id_or_error);

						if (std::holds_alternative<extended_ast::atom_type>(converted_child))
						{
							elements.push_back(extended_ast::atom_declaration{
								std::move(std::get<extended_ast::atom_type>(converted_child)),
								std::move(std::get<extended_ast::identifier>(converted_id))
							});
						}
						else if (std::holds_alternative<extended_ast::function_type>(converted_child))
						{
							elements.push_back(extended_ast::function_declaration{
								std::move(std::get<extended_ast::function_type>(converted_child)),
								std::move(std::get<extended_ast::identifier>(converted_id))
							});
						}
						else
						{
							return cst_to_ast_error{ "Variable declaration can only contain atom types and function types" };
						}
					}

					return extended_ast::tuple_declaration{
						std::move(elements)
					};
				}
				else if (node_type == type_tuple)
				{
					// Type tuple has children [type_expression*]
					std::vector<std::variant<extended_ast::atom_type, extended_ast::function_type>> elements;

					for (decltype(auto) child : n.children)
					{
						auto converted_child_or_error = convert(std::move(child));
						if (holds_error(converted_child_or_error)) return get_error(converted_child_or_error);
						auto& converted_child = std::get<extended_ast::node>(converted_child_or_error);

						if (std::holds_alternative<extended_ast::atom_type>(converted_child))
						{
							elements.push_back(std::move(std::get<extended_ast::atom_type>(converted_child)));
						}
						else if (std::holds_alternative<extended_ast::function_type>(converted_child))
						{
							elements.push_back(std::move(std::get<extended_ast::function_type>(converted_child)));
						}
						else
						{
							return cst_to_ast_error{ "Type tuple can only contain atom and function declarations" };
						}
					}

					return extended_ast::tuple_type{
						std::move(elements)
					};
				}
				else if (node_type == type_function)
				{
					// Type Function has children [type_expression, type_expression]
					auto converted_from = convert(std::move(n.children.at(0)));
					if (holds_error(converted_from)) return get_error(converted_from);
					auto& type_from_node = get_node(converted_from);

					auto converted_to = convert(std::move(n.children.at(1)));
					if (holds_error(converted_to)) return get_error(converted_to);
					auto& type_to_node = get_node(converted_to);

					return extended_ast::function_type{
						std::move(std::get<extended_ast::tuple_type>(type_from_node)),
						std::move(std::get<extended_ast::tuple_type>(type_to_node))
					};
				}
				else if (node_type == type_expression)
				{
					if (std::holds_alternative<tools::ebnfe::terminal_node>(*n.children.at(0)))
					{
						auto converted_first = convert(std::move(n.children.at(0)));
						if (holds_error(converted_first)) return get_error(converted_first);
						auto& first = get_node(converted_first);

						return extended_ast::atom_type{
							std::move(std::get<extended_ast::identifier>(first))
						};
					}
					else
					{
						return convert(std::move(n.children.at(0)));
					}
				}
				else if (node_type == function)
				{
					auto from = convert(std::move(n.children.at(0)));
					if (holds_error(from)) return get_error(from);

					auto to = convert(std::move(n.children.at(1)));
					if (holds_error(to)) return get_error(to);

					auto body = convert(std::move(n.children.at(2)));
					if (holds_error(body)) return get_error(body);

					return extended_ast::function{
						std::optional<extended_ast::identifier>(),
						std::move(std::get<extended_ast::tuple_declaration>(std::move(get_node(from)))),
						extended_ast::make_unique(std::move(get_node(to))),
						extended_ast::make_unique(std::move(get_node(body)))
					};
				}
				else if (node_type == branch)
				{
					// Branch has children [branch_elements*]

					std::vector<extended_ast::conditional_branch_path> branches;
					for (auto&& child : n.children)
					{
						auto converted_branch = convert(std::move(child));
						if (holds_error(converted_branch)) return get_error(converted_branch);

						branches.push_back(std::move(std::get<extended_ast::conditional_branch_path>(get_node(std::move(converted_branch)))));
					}
					return extended_ast::conditional_branch(std::move(branches));
				}
				else if (node_type == branch_element)
				{
					auto test_path = convert(std::move(n.children.at(0)));
					if (holds_error(test_path)) return get_error(test_path);

					auto code_path = convert(std::move(n.children.at(1)));
					if (holds_error(code_path)) return get_error(code_path);

					return extended_ast::conditional_branch_path{
						extended_ast::make_unique(get_node(test_path)),
						extended_ast::make_unique(get_node(code_path))
					};
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

				if (node_type == number)
				{
					return extended_ast::integer(atoi(n.token.c_str()));
				}
				else if (node_type == word)
				{
					return extended_ast::string(n.token.substr(1, n.token.size() - 2));
				}
				else if (node_type == identifier)
				{
					auto splitter = [](std::string identifier) -> std::vector<std::string> {
						std::vector<std::string> split_identifier;

						bool reading_infix = false;
						std::string::iterator begin_word = identifier.begin();
						for (auto it = identifier.begin(); it != identifier.end(); it++)
						{
							if (*it == ':')
							{
								if (!reading_infix) reading_infix = true;
								else
								{ // Read infix
									split_identifier.push_back(std::string(begin_word, it - 1));
									begin_word = it + 1;
									continue;
								}
							}
							else
							{
								if (reading_infix) reading_infix = false;
								if (it == identifier.end() - 1)
									split_identifier.push_back(std::string(begin_word, it + 1));
							}
						}
						return split_identifier;
					};

					auto words = splitter(n.token);
					return extended_ast::identifier(std::move(words));
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
		bool holds_error(const std::variant<extended_ast::node, cst_to_ast_error>& node)
		{
			return std::holds_alternative<cst_to_ast_error>(node);
		}

		cst_to_ast_error get_error(std::variant<extended_ast::node, cst_to_ast_error>& node)
		{
			return std::get<cst_to_ast_error>(node);
		}

		extended_ast::node& get_node(std::variant<extended_ast::node, cst_to_ast_error>& node)
		{
			return std::get<extended_ast::node>(node);
		}
	};
}
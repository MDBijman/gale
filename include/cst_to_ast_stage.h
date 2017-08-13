#pragma once
#include "pipeline.h"
#include "language_definition.h"

namespace fe
{
	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<tools::ebnfe::node>, extended_ast::node_v>
	{
	public:
		extended_ast::node_v convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto n = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));
				auto node_type = n.value;

				if (node_type == file)
				{
					// File has children [statements*]

					std::vector<extended_ast::node_v> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						auto new_node{ convert(std::move(n.children.at(i))) };
						children.push_back(std::move(new_node));
					}
					return extended_ast::value_tuple(std::move(children));
				}
				else if (node_type == assignment)
				{
					// Assignment has children [identifier value]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));

					// Convert the value
					auto converted_value = convert(std::move(n.children.at(1)));

					auto assignment = extended_ast::assignment(
						std::move(std::get<extended_ast::identifier>(converted_identifier)),
						std::move(converted_value)
					);

					return assignment;
				}
				else if (node_type == expression)
				{
					// TODO create seperate nt for function
					// Expression has children [value_tuple | number | word | identifier [value_tuple]]

					// Function call
					if (n.children.size() == 2)
					{
						// Convert the identifier
						auto converted_identifier = convert(std::move(n.children.at(0)));

						// Convert the tuple
						auto converted_tuple = convert(std::move(n.children.at(1)));

						return extended_ast::function_call(
							std::move(std::get<extended_ast::identifier>(converted_identifier)),
							std::move(std::get<extended_ast::value_tuple>(converted_tuple))
						);
					}
					else
					{
						return convert(std::move(n.children.at(0)));
					}
				}
				else if (node_type == value_tuple)
				{
					// Tuple has children [values*]

					std::vector<extended_ast::node_v> children;
					for (unsigned int i = 0; i < n.children.size(); i++)
					{
						children.push_back(convert(std::move(n.children.at(i))));
					}
					return extended_ast::value_tuple(std::move(children));
				}
				else if (node_type == type_definition)
				{
					// Type definition has children [identifier type_tuple]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));

					// Convert the tuple
					auto converted_tuple = convert(std::move(n.children.at(1)));

					return extended_ast::type_declaration(
						std::move(std::get<extended_ast::identifier>(converted_identifier)),	
						std::move(std::get<extended_ast::type_tuple>(converted_tuple))
					);
				}
				else if (node_type == export_stmt)
				{
					// Export has children [identifier]

					// Convert the identifier
					auto converted_identifier = convert(std::move(n.children.at(0)));

					return extended_ast::export_stmt(
						std::move(std::get<extended_ast::identifier>(converted_identifier))
					);
				}
				else if (node_type == type_tuple)
				{
					// Type tuple has children [types*]

					std::vector<extended_ast::node_v> children;
					for (decltype(auto) child : n.children)
					{
						children.push_back(std::move(convert(std::move(child))));
					}

					return extended_ast::type_tuple(std::move(children));
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
								if(it == identifier.end() - 1)
									split_identifier.push_back(std::string(begin_word, it + 1));
							}
						}
						return split_identifier;
					};

					auto words = splitter(n.token);
					return extended_ast::identifier(std::move(words));
				}
			}

			throw std::runtime_error("Unknown node type");
		}
	};
}
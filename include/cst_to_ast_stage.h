#pragma once
#include "pipeline.h"
#include "language.h"

namespace fe
{
	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<tools::ebnfe::node>, extended_ast::node_p>
	{
	public:
		extended_ast::node_p convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto n = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));

				if (n.value == file)
				{
					// File has children [assignment]

					return convert(std::move(n.children.at(0)));
				}
				else if (n.value == assignment)
				{
					// Assignment has children [identifier constructor_or_identifier]

					// Convert the identifier
					auto converted_identifier =
						dynamic_cast<extended_ast::identifier*>(convert(std::move(n.children.at(0))).release());

					// Convert the value
					auto converted_value = convert(std::move(n.children.at(1)));

					return std::make_unique<extended_ast::assignment>(
						std::move(*converted_identifier),
						std::move(converted_value)
					);
				}
				else if (n.value == expression)
				{
					// Expression has children [tuple_t | number | word | identifier [tuple]]

					// Function call
					if (n.children.size() == 2)
					{
						// Convert the identifier
						auto converted_identifier =
							dynamic_cast<extended_ast::identifier*>(convert(std::move(n.children.at(0))).release());

						// Convert the tuple
						auto converted_tuple =
							dynamic_cast<extended_ast::tuple*>(convert(std::move(n.children.at(1))).release());

						return std::make_unique<extended_ast::function_call>(
							std::move(*converted_identifier),
							std::move(*converted_tuple)
						);
					}
					else
					{
						return convert(std::move(n.children.at(0)));
					}
				}
				else if (n.value == tuple_t)
				{
					// Tuple has children [values...]

					auto values = std::make_unique<extended_ast::tuple>();
					for (int i = 0; i < n.children.size(); i++)
					{
						values->get_children().push_back(std::move(convert(std::move(n.children.at(i)))));
					}
					return values;
				}
			}
			else if (std::holds_alternative<tools::ebnfe::terminal_node>(*node))
			{
				auto n = std::move(std::get<tools::ebnfe::terminal_node>(*node));

				if (n.value == number)
				{
					return std::make_unique<extended_ast::integer>(atoi(n.token.c_str()));
				}
				else if (n.value == word)
				{
					return std::make_unique<extended_ast::string>(n.token.substr(1, n.token.size() - 2));
				}
				else if (n.value == identifier)
				{
					return std::make_unique<extended_ast::identifier>(std::move(n.token), types::void_type());
				}
			}
			return nullptr;
		}
	};
}
#pragma once
#include "pipeline.h"
#include "language.h"

namespace fe
{
	struct type_environment
	{
	private:
		std::unordered_map<std::string, std::unique_ptr<types::type>> types;
	};

	class cst_to_ast_stage : public language::cst_to_ast_stage<std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<fe::ast::node>>
	{
	public:
		type_environment t_env;

		std::unique_ptr<ast::node> convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto nt_node = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));

				if (nt_node.value == file)
				{
					// File has children [assignment]

					auto converted_assignment = convert(std::move(nt_node.children.at(0)));

					return converted_assignment;
				}
				else if (nt_node.value == tuple_t)
				{
					// Tuple has children [values...]

					auto values = std::make_unique<ast::tuple>();
					for (int i = 0; i < nt_node.children.size(); i++) 
					{
						values->children.push_back(std::move(convert(std::move(nt_node.children.at(i)))));
					}
					return std::move(values);
				}
				else if (nt_node.value == data)
				{
					// Data has children [tuple | number | string]
					return std::move(convert(std::move(nt_node.children.at(0))));
				}
				else if (nt_node.value == assignment)
				{
					// Assignment has children [identifier typename value]

					// Convert the identifier
					auto converted_identifier = 
						std::unique_ptr<ast::identifier>(
							dynamic_cast<ast::identifier*>(convert(std::move(nt_node.children.at(0))).release())
						);

					// Convert the typename
					auto converted_typename = 
						std::unique_ptr<ast::identifier>(
							dynamic_cast<ast::identifier*>(convert(std::move(nt_node.children.at(1))).release())
						);

					// Convert the value
					auto converted_value = 
						std::unique_ptr<ast::tuple>(
							dynamic_cast<ast::tuple*>(convert(std::move(nt_node.children.at(2))).release())
						);

					auto assignment = std::make_unique<ast::assignment>(
						std::move(converted_identifier),
						std::move(converted_typename),
						std::move(converted_value)
					);

					return std::move(assignment);
				}
			}
			else if (std::holds_alternative<tools::ebnfe::terminal_node>(*node))
			{
				auto t_node = std::move(std::get<tools::ebnfe::terminal_node>(*node));

				if (t_node.value == number)
				{
					return std::make_unique<ast::integer>(atoi(t_node.token.c_str()));
				}
				else if (t_node.value == word)
				{
					return std::make_unique<ast::string>(t_node.token.substr(1, t_node.token.size() - 2));
				}
				else if (t_node.value == identifier)
				{
					return std::make_unique<ast::identifier>(t_node.token);
				}
			}
			return nullptr;
		}
	};
}
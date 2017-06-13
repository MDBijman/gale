#pragma once
#include "pipeline.h"
#include "language.h"

namespace fe
{
	class parsing_to_lowering_stage : public language::parsing_to_lowering_stage<std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<fe::ast::node>>
	{
	public:
		std::unique_ptr<fe::ast::node> convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				tools::ebnfe::non_terminal_node nt_node = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));

				if (nt_node.value == file)
				{
					std::unique_ptr<ast::file> node = std::make_unique<ast::file>();

					for (auto& child : nt_node.children)
					{
						node->children.push_back(convert(std::move(child)));
					}

					return node;
				}
				else if (nt_node.value == statement)
				{
					return convert(std::move(nt_node.children.at(0)));
				}
				else if (nt_node.value == assignment)
				{
					auto identifier = ast::identifier{ std::get<tools::ebnfe::terminal_node>(*nt_node.children.at(0)).token };

					return std::make_unique<ast::assignment>(identifier, convert(std::move(nt_node.children.at(1))));
				}
				else if (nt_node.value == print)
				{
					auto identifier = ast::identifier("print");

					std::vector<std::unique_ptr<ast::node>> parameters;
					parameters.push_back(std::make_unique<ast::identifier>(std::get<tools::ebnfe::terminal_node>(*nt_node.children.at(0)).token));

					return std::make_unique<ast::function_call>(identifier, std::move(parameters));
				}
			}
			else if (std::holds_alternative<tools::ebnfe::terminal_node>(*node))
			{
				tools::ebnfe::terminal_node t_node = std::move(std::get<tools::ebnfe::terminal_node>(*node));

				if (t_node.value == identifier)
				{
					return std::make_unique<ast::identifier>(t_node.token);
				}
				else if (t_node.value == number)
				{
					return std::make_unique<ast::number>(atoi(t_node.token.c_str()));
				}
			}
			return nullptr;
		}
	};
}
#pragma once
#include "pipeline.h"
#include "language.h"

namespace fe
{
	class parsing_to_lowering_stage : public language::parsing_to_lowering_stage<std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<fe::ast::node>>
	{
	public:
		std::unique_ptr<ast::node> convert(std::unique_ptr<tools::ebnfe::node> node)
		{
			if(std::holds_alternative<tools::ebnfe::non_terminal_node>(*node))
			{
				auto nt_node = std::move(std::get<tools::ebnfe::non_terminal_node>(*node));

				if (nt_node.value == file)
				{
					auto converted_assignment = convert(std::move(nt_node.children.at(0)));

					return converted_assignment;
				}
				else if (nt_node.value == tuple_t)
				{
					auto values = std::make_unique<ast::tuple>();
					for (int i = 0; i < nt_node.children.size(); i++) 
					{
						values->children.push_back(std::move(convert(std::move(nt_node.children.at(i)))));
					}
					return std::move(values);
				}
				else if (nt_node.value == data)
				{
					return std::move(convert(std::move(nt_node.children.at(0))));
				}
				else if (nt_node.value == assignment)
				{
					auto converted_identifier = std::move(convert(std::move(nt_node.children.at(0))));
					auto converted_value = std::move(convert(std::move(nt_node.children.at(1))));

					return std::move(std::make_unique<ast::assignment>(
						std::move(*dynamic_cast<ast::identifier*>(converted_identifier.get())), 
						std::move(converted_value)
					));
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
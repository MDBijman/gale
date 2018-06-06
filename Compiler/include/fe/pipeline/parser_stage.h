#pragma once
#include "fe/language_definition.h"
#include "utils/parsing/ebnfe_parser.h"

#include <memory>

namespace fe
{
	class parsing_strategy
	{
	public:
		virtual ~parsing_strategy() {}
		virtual std::variant<utils::bnf::tree, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) = 0;
	};

	class lr_strategy : public parsing_strategy
	{
		utils::ebnfe::parser parser;
	public:
		lr_strategy();
		virtual std::variant<utils::bnf::tree, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) override;
	};

	class recursive_descent_strategy : public parsing_strategy
	{
	public:
		virtual std::variant<utils::bnf::tree, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) override;
	};
	
	class parsing_stage 
	{
		std::unique_ptr<parsing_strategy> strategy;

	public:
		parsing_stage(std::unique_ptr<parsing_strategy> imp);

		std::variant<utils::bnf::tree, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in);
	};
}
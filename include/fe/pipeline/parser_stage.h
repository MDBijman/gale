#pragma once
#include "fe/language_definition.h"
#include "utils/parsing/ebnfe_parser.h"

#include <memory>

namespace fe
{
	class parsing_stage 
	{
	public:
		parsing_stage();

		std::variant<utils::bnf::tree, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) 
		{
			parser.generate(fe::non_terminals::file);
			return parser.parse(in);
		}

	private:
		utils::ebnfe::parser parser;
	};
}
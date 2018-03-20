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

		std::variant<std::unique_ptr<utils::ebnfe::node>, utils::ebnfe::error> parse(const std::vector<utils::bnf::terminal_node>& in) 
		{
			return parser.parse(fe::non_terminals::file, in);
		}

	private:
		utils::ebnfe::parser parser;
	};
}
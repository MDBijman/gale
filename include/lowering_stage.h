#pragma once
#include "ebnfe_parser.h"
#include <memory>

namespace fe
{
	class lowering_stage : public language::lowering_stage<std::unique_ptr<fe::ast::node>, std::unique_ptr<fe::ast::node>>
	{
	public:
		std::unique_ptr<fe::ast::node> lower(std::unique_ptr<fe::ast::node> n)
		{
			return n;
		}
	};
}
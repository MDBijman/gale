#pragma once
#include "ebnfe_parser.h"
#include <memory>

namespace fe
{
	class lowering_stage : public language::lowering_stage<std::unique_ptr<tools::ebnfe::node>, std::unique_ptr<tools::ebnfe::node>>
	{
	public:
		std::unique_ptr<tools::ebnfe::node> lower(std::unique_ptr<tools::ebnfe::node> n)
		{
			return n;
		}
	};
}
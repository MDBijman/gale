#pragma once
#include "ebnfe_parser.h"

namespace language
{
	namespace fe
	{
		class desugarer
		{
		public:
			ebnfe::node* desugar(ebnfe::node* n)
			{
				return n;
			}
		};
	}
}
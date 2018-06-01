#include "utils/parsing/bnf_grammar.h"
#include <stack>

namespace utils::bnf
{
	bool operator==(const rule& r1, const rule& r2)
	{
		return r1.first == r2.first && (r1.second == r2.second);
	}
}
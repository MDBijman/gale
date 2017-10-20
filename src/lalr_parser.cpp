#include "lalr_parser.h"

namespace tools::lalr
{
	void parser::generate_item_sets()
	{
		item_sets.push_back(item_set());

		auto expand_item_set = [&](item_set& i) {
			for (const auto& rule : i.rules)
			{
				const auto& rhs = rule.second;
				if (rhs.bullet_offset >= rhs.symbols.size())
					continue;

				auto& expected_symbol = rhs.symbols.at(rhs.bullet_offset);
				if (expected_symbol.is_terminal())
					continue;

				// Bullet is not too far right and the symbol after it is a non terminal
				auto& expected_rule = rules.at(expected_symbol.get_non_terminal());
				i.rules.insert({ expected_symbol.get_non_terminal(), lalr_rhs{ expected_rule, 0 } });
			}
		};

		{
			auto& i0 = item_sets.at(0);
			i0.rules.insert({ start_symbol, lalr_rhs{ rules.at(start_symbol), 0 } });
			expand_item_set(i0);
		}
	}
}
#pragma once
#include "fe/data/module.h"
#include <assert.h>

namespace testing
{
	class test_scope
	{
		fe::module m;
	public:
		test_scope(fe::module m) : m(m) {}

		template<typename ValueType>
		bool value_equals(std::string name, ValueType val)
		{
			auto lookup = m.values.valueof(fe::core_ast::identifier({}, name, 0, {}));
			if (!lookup) return false;
			auto value = dynamic_cast<ValueType*>(*lookup);
			if (!value) return false;
			return *value == val;
		}
	};
}
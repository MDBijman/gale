#pragma once
#include "fe/data/scope.h"
#include <assert.h>

namespace testing
{
	class test_scope
	{
		fe::scope scope;
	public:
		test_scope(fe::scope s) : scope(s) {}

		template<typename ValueType>
		bool value_equals(std::string name, ValueType val)
		{
			auto lookup = scope.value_env().valueof(fe::core_ast::identifier({}, name, 0, {}), 0,
				[](fe::scope_index) { return nullptr; });
			assert(lookup);
			auto value = dynamic_cast<ValueType*>(*lookup);
			assert(value);
			return *value == val;
		}
	};
}
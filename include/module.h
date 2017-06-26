#pragma once
#include <unordered_map>

#include "types.h"
#include "values.h"

namespace language
{
	class module
	{
	public:
		virtual std::unordered_map<std::string, fe::types::type> get_types() = 0;
		virtual std::unordered_map<std::string, std::shared_ptr<fe::values::value>> get_values() = 0;
	};
}
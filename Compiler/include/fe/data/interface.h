#pragma once
#include "fe/data/name_scope.h"
#include "fe/data/type_scope.h"

#include <string>
#include <vector>

namespace fe
{
	struct interface
	{
		interface()
		{
		}

		interface(std::string n, ext_ast::type_scope ts, ext_ast::name_scope ns)
		    : name(n), types(ts), names(ns)
		{
		}

		std::string name;
		ext_ast::type_scope types;
		ext_ast::name_scope names;
	};

	using interfaces = std::vector<interface>;

} // namespace fe
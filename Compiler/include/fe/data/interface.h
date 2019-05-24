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

		interface(std::string n, std::vector<std::string> i, ext_ast::type_scope ts, ext_ast::name_scope ns)
		    : name(n), imports(i), types(ts), names(ns)
		{
		}

		std::string name;
		std::vector<std::string> imports;
		ext_ast::type_scope types;
		ext_ast::name_scope names;
	};

	using interfaces = std::vector<interface>;

} // namespace fe
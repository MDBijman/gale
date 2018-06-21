#pragma once

#include "fe/data/ext_ast.h"
#include "fe/data/value_scope.h"
#include "fe/data/type_scope.h"
#include "fe/data/constants_store.h"

namespace fe
{
	using module_name = std::vector<std::string>;

	struct module
	{
		module_name name;
		core_ast::value_scope values;
		ext_ast::type_scope types;
		ext_ast::name_scope names;
		constants_store constants;
	};
}
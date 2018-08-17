#pragma once
#include "fe/data/ext_ast.h"
#include "fe/data/type_scope.h"
#include "fe/data/bytecode.h"
#include "fe/data/constants_store.h"

namespace fe
{
	using module_name = std::vector<std::string>;

	struct module
	{
		module() {}
		module(module_name n, ext_ast::type_scope ts, ext_ast::name_scope ns, constants_store cs, vm::module c) :
			name(n), types(ts), names(ns), constants(cs), code(c) {}

		module_name name;
		ext_ast::type_scope types;
		ext_ast::name_scope names;
		constants_store constants;
		vm::module code;
	};

	class module_builder
	{
		module m;

	public:
		module_builder() {}

		module_builder& set_name(module_name mn)
		{
			m.name = mn;
			return *this;
		}

		module_builder& add_function(std::string name, types::unique_type t, vm::bytecode b)
		{
			m.names.declare_variable(name, -1);
			m.names.define_variable(name);
			m.types.set_type(name, std::move(t));
			m.code.push_back(vm::function{ name, b });
			return *this;
		}
		
		module build() { return m; }
	};
}

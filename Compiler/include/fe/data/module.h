#pragma once
#include "fe/data/bytecode.h"
#include "fe/data/constants_store.h"
#include "fe/data/ext_ast.h"
#include "fe/data/type_scope.h"
#include "fe/vm/runtime_info.h"

#include <vector>

namespace fe
{
	using module_name = std::vector<std::string>;

	struct module
	{
		module()
		{
		}
		module(module_name n, ext_ast::type_scope ts, ext_ast::name_scope ns, vm::module c)
		    : name(n), types(ts), names(ns), code(c)
		{
		}

		module_name name;
		ext_ast::type_scope types;
		ext_ast::name_scope names;
		vm::module code;
	};

	class module_builder
	{
		module m;

	      public:
		module_builder()
		{
		}

		module_builder &set_name(module_name mn)
		{
			m.name = mn;
			return *this;
		}

		module_builder &add_function(vm::function f, types::unique_type t)
		{
			m.names.declare_variable(f.get_name(), -1);
			m.names.define_variable(f.get_name());
			m.types.set_type(f.get_name(), std::move(t));
			m.code.add_function(f);
			return *this;
		}

		module_builder &add_native_function(vm::native_function_id id,
						    const std::string &name, types::unique_type t)
		{
			m.names.declare_variable(name, -1);
			m.names.define_variable(name);
			m.types.set_type(name, std::move(t));
			m.code.add_function(fe::vm::function(name, id));
			return *this;
		}

		module_builder &add_type(const std::string &name, types::unique_type t)
		{
			m.names.define_type(name, -1);
			m.types.define_type(name, std::move(t));
			return *this;
		}

		module build()
		{
			return m;
		}
	};
} // namespace fe

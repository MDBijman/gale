#pragma once
#include "fe/data/bytecode.h"
#include "fe/data/constants_store.h"
#include "fe/data/ext_ast.h"
#include "fe/data/type_scope.h"
#include "interface.h"
#include "fe/vm/runtime_info.h"

#include <vector>

namespace fe
{
	struct module
	{
		module()
		{
		}
		module(interface iface, vm::module imp) : iface(iface), implementation(imp)
		{
		}

		interface iface;
		vm::module implementation;
	};

	class module_builder
	{
		module m;

	      public:
		module_builder()
		{
		}

		module_builder &set_name(std::string mn)
		{
			m.iface.name = mn;
			return *this;
		}

		module_builder &add_function(vm::function f, types::unique_type t)
		{
			m.iface.names.declare_variable(f.get_name(), -1);
			m.iface.names.define_variable(f.get_name());
			m.iface.types.set_type(f.get_name(), std::move(t));
			m.implementation.add_function(f);
			return *this;
		}

		module_builder &add_native_function(vm::native_function_id id,
						    const std::string &name, types::unique_type t)
		{
			m.iface.names.declare_variable(name, -1);
			m.iface.names.define_variable(name);
			m.iface.types.set_type(name, std::move(t));
			m.implementation.add_function(fe::vm::function(name, id));
			return *this;
		}

		module_builder &add_type(const std::string &name, types::unique_type t)
		{
			m.iface.names.define_type(name, -1);
			m.iface.types.define_type(name, std::move(t));
			return *this;
		}

		module build()
		{
			return m;
		}
	};
} // namespace fe

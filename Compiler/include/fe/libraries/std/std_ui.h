#pragma once
#include "fe/data/types.h"
#include "fe/data/module.h"
#include <Windows.h>

namespace fe::stdlib::ui
{
	static module load()
	{
		ext_ast::name_scope se;
		ext_ast::type_scope te;
		constants_store constants;

		// Create Window
		{
			using namespace types;
			function_type create_window_type{ types::str(), types::any() };

			auto& create_window_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			create_window_ref.full = "create_window";
			se.declare_variable(create_window_ref.full);
			se.define_variable(create_window_ref.full);
			te.set_type(create_window_ref.full, unique_type(create_window_type.copy()));
		}

		// Poll
		{
			using namespace types;
			auto& poll_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			poll_ref.full = "poll";
			se.declare_variable(poll_ref.full);
			se.define_variable(poll_ref.full);
			te.set_type(poll_ref.full, unique_type(new function_type(types::any(), types::voidt())));
		}

		return module{
			{"std","ui"},
			std::move(te),
			std::move(se),
			std::move(constants)
		};
	}
}
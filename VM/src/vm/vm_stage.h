#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	struct vm_settings
	{
		bool direct_thread = false;
	};

	void interpret(executable&, vm_settings& = vm_settings{});
}
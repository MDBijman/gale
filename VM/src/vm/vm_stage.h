#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	struct vm_settings
	{

	};

	void interpret(executable&, vm_settings& = vm_settings{});
}
#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	struct optimization_settings
	{

	};

	void optimize(executable& p, optimization_settings s);
}
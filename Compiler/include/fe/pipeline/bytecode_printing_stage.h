#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	void print_bytecode(const std::string& path, executable& e);
}

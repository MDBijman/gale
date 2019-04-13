#pragma once
#include "fe/data/bytecode.h"

#include <unordered_map>
#include <string>

namespace fe::vm
{
	executable link(const std::unordered_map<std::string, module>& modules);
}

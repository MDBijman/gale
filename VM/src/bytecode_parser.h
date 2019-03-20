#pragma once
#include "fe/data/bytecode.h"
#include <string>

namespace fe::vm
{
	/*
	* Parses a bytecode file located at the given filename.
	* Returns an executable containing the parsed bytecode instructions.
	*/
	executable parse_bytecode(const std::string& filename);
}
#include "bytecode_parser.h"
#include "vm/vm_stage.h"
#include <iostream>

int main(int argc, char **argv)
{
	if (argc > 2)
	{
		std::cerr << "Expected only a single bytecode file as argument\n";
		std::exit(1);
	}
	else if (argc == 1)
	{
		std::cerr << "Expected a single bytecode file as argument\n";
		std::exit(1);
	}

	auto filename = std::string(argv[1]);
	auto executable = fe::vm::parse_bytecode(filename);

	if (executable.byte_length() == 0)
	{
		std::cerr << "Bytecode is empty\n";
		std::exit(1);
	}

	fe::vm::interpret(executable);

	return 0;
}
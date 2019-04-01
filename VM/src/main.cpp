#include "bytecode_parser.h"
#include "vm/vm_stage.h"
#include <iostream>

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "Expected a single bytecode file as input\n";
		std::exit(1);
	}

	if (argv[1] != "-i")
	{
		std::cerr << "Expected a single bytecode file as input\n";
		std::exit(1);
	}

	auto filename = std::string(argv[2]);
	auto executable = fe::vm::parse_bytecode(filename);

	if (executable.byte_length() == 0)
	{
		std::cerr << "Bytecode is empty\n";
		std::exit(1);
	}

	fe::vm::interpret(executable);

	return 0;
}
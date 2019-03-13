#include "vm_stage.h"
#include "bytecode_parser.h"
#include <iostream>

int main(int argc, char** argv)
{
	if (argc > 2)
	{
		std::cout << "Expected only a single bytecode file as argument\n";
		std::exit(1);
	}
	else if(argc == 1)
	{
		std::cout << "Expected a single bytecode file as argument\n";
		std::exit(1);
	}

	std::string filename(argv[1]);

	auto exec = fe::vm::parse_bytecode(filename);
	std::cout << exec.to_string();
	fe::vm::interpret(exec);
	return 0;
}
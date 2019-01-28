#include "vm_stage.h"
#include "bytecode_parser.h"
#include <iostream>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cout << "Unexpected number of input arguments";
		std::exit(1);
	}

	std::string filename(argv[1]);

	auto exec = fe::vm::parse_bytecode(filename);
	std::cout << exec.to_string();
	fe::vm::interpret(exec);
	std::cin.get();
	return 0;
}
#include "parser.h"
#include "desugarer.h"
#include "interpreter.h"

#include <stdio.h>
#include <functional>
#include <algorithm>
#include <fstream>
#include <iostream>


std::function<void(int, language::ebnfe::node*)> print_ast = [&](int indentation, language::ebnfe::node* node) {
	for (int i = 0; i < indentation; i++)
		std::cout << "\t";
	if (node->value.is_terminal())
		std::cout << node->value.get_terminal() << std::endl;
	else
	{
		std::cout << node->value.get_non_terminal() << std::endl;
		for (auto child : node->children)
			print_ast(indentation + 1, child);
	}
};

int main()
{
	// Parse rules
	std::ifstream in("./snippets/example.fe", std::ios::in | std::ios::binary);
	if (!in) throw std::exception("Could not open file");

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();
	std::string_view contents_view = contents;

	language::fe::parser parser;
	auto parsed_ast = parser.parse(contents);

	std::cout << "Parsed AST" << std::endl;
	print_ast(0, parsed_ast);

	language::fe::desugarer desugarer;
	auto desugared_ast = desugarer.desugar(parsed_ast);

	std::cout << "Desugared AST" << std::endl;
	print_ast(0, parsed_ast);

	language::fe::interpreter interpreter;
	std::cout << "Program Output" << std::endl;
	interpreter.interp(desugared_ast);
	
	std::cin.get();
}
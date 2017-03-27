#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "parser.h"
#include "fe_parser.h"

int main()
{
	char* source = nullptr;
	FILE* file = nullptr;

	errno_t error = fopen_s(&file, "./test.fe", "r");
	if (!file || error) return -1;

	// Move to end
	if (fseek(file, 0L, SEEK_END))
	{
		fclose(file);
		return -2;
	}

	// Get file size
	long filesize = ftell(file);
	if (filesize == -1)
	{
		fclose(file);
		return -3;
	}
	
	if (fseek(file, 0L, SEEK_SET))
	{
		fclose(file);
		return -4;
	}

	source = (char*)malloc(sizeof(char) * filesize);
	size_t newLen = fread(source, sizeof(char), filesize, file);
	if (ferror(file) != 0)
	{
		return -5;
	}

	source[newLen] = '\0';

	std::string source_file{ source, newLen };

	parser::parser parser(std::move(source));

	parser.parse(new global_reader);

	std::cin.get();
}
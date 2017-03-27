#pragma once
#include "parser.h"
#include <assert.h>

class macro_reader : public parser::abstract_state
{
public:
	void read(parser::state_machine& machine, parser::string_reader& t) override
	{
		auto identifier = t.consume_token();
		std::cout << identifier << std::endl;
		assert(t.consume_token().compare("{") == 0);
		assert(t.consume_token().compare("from") == 0);
		assert(t.consume_token().compare("{") == 0);
		// from parse



		assert(t.consume_token().compare("}") == 0);
		assert(t.consume_token().compare("to") == 0);
		// to parse
		assert(t.consume_token().compare("{") == 0);
		assert(t.consume_token().compare("}") == 0);
		assert(t.consume_token().compare("}") == 0);
	}
};

class global_reader : public parser::abstract_state
{
public:
	void read(parser::state_machine& machine, parser::string_reader& t) override
	{
		t.consume_whitespace();

		if (t.read_equals("macro"))
		{
			t.consume_token();
			machine.transition(new macro_reader);
		}
		else
		{
			machine.exit();
		}
	}
};
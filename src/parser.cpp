#include "parser.h"

#include <string.h>

namespace parser
{
	string_reader::string_reader(std::string::const_iterator it) : iterator(it) {}

	void string_reader::consume_whitespace()
	{
		while (isspace(*iterator)) iterator++;
	}

	std::string_view string_reader::consume_token()
	{
		consume_whitespace();

		auto temp = iterator;
		// Advance until space
		while (!isspace(*iterator++));
	
		return std::string_view(&(*temp), std::distance(temp, iterator));
	}

	std::string_view string_reader::read_token()
	{
		consume_whitespace();

		auto temp = iterator;
		// Advance until space
		while (!isspace(*++temp));

		return std::string_view(&(*iterator), std::distance(iterator, temp));
	}

	bool string_reader::read_equals(const char* str)
	{
		auto token = read_token();
		return token.compare(str) == 0;
	}
	
	state_machine::state_machine(abstract_state* initial_state) : state(initial_state) {}

	abstract_state* state_machine::current_state()
	{
		return this->state;
	}

	void state_machine::set_state(abstract_state* new_state)
	{
		if (finished) return;
		delete this->state;
		this->state = new_state;
	}

	void state_machine::transition(abstract_state* new_state)
	{
		if (finished) return;
		if (new_state == nullptr) throw new std::exception("Cannot transition to null state.");
		this->set_state(new_state);
	}

	void state_machine::exit()
	{
		if (finished) return;
		this->set_state(nullptr);
		finished = true;
	}

	bool state_machine::is_finished()
	{
		return this->finished;
	}
	
	parser::parser(std::string&& s) : source(std::move(s)) {}

	void parser::parse(abstract_state* initial_state)
	{
		state_machine reader(initial_state);
		string_reader tokenizer(source.begin());
		
		while(!reader.is_finished()) reader.current_state()->read(reader, tokenizer);
	}
}
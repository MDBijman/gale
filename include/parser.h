#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <string_view>

namespace parser
{
	class string_reader
	{
	public:
		string_reader(std::string::const_iterator it);

		// Consume until next non-whitespace
		void consume_whitespace();

		// Consume until next whitespace
		std::string_view consume_token();

		// Read next token but don't advance
		std::string_view read_token();

		// True if the next token matches the string
		bool read_equals(const char* str);

	private:
		std::string::const_iterator iterator;
	};

	class abstract_state;
	class state_machine
	{
	public:
		state_machine(abstract_state* state);
			
		abstract_state* current_state();

		void transition(abstract_state* new_state);
		void exit();
		bool is_finished();

	private:
		void set_state(abstract_state* new_state);
		bool finished = false;
		abstract_state* state = nullptr;
	};

	class abstract_state
	{
	public:
		virtual void read(state_machine& machine, string_reader& t) = 0;
	};

	using token = std::string;

	class parser
	{
	public:
		parser(std::string&& s);

		void parse(abstract_state*);

	private:
		std::string source;
	};
}

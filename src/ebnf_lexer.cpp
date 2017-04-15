#include "ebnf_lexer.h"
#include <iostream>

namespace ebnf
{
	void state_decider::run(state_machine& machine)
	{
		while (iterator != line.end() && isspace(*iterator)) iterator++;
		if (iterator == line.end())
		{
			machine.exit();
			return;
		}

		if (*iterator == '|')
			machine.transition(new alternation_state(line, iterator, tokens));
		else if (*iterator == '.')
			machine.transition(new end_of_rule_state(line, iterator, tokens));
		else if (*iterator == '\'')
			machine.transition(new string_state(line, iterator, tokens));
		else if (isalpha(*iterator))
			machine.transition(new identifier_state(line, iterator, tokens));
		else if (*iterator == '+' || *iterator == '*' || *iterator == '?')
			machine.transition(new quantifier_state(line, iterator, tokens));
		else if (*iterator == '(' || *iterator == ')')
			machine.transition(new group_state(line, iterator, tokens));
		else if (isalpha(*iterator))
			machine.transition(new identifier_state(line, iterator, tokens));
		else if (*iterator == ':')
			machine.transition(new assignment_state(line, iterator, tokens));
		else if (*iterator == '-')
			machine.transition(new exception_state(line, iterator, tokens));
		else
		{
			std::cerr << "Error: unknown symbol: \'" << *iterator << "\'." << std::endl;
			throw std::exception("Unknown symbol.");
		}
	}
}

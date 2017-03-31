#include "state_machine.h"
#include <exception>

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
#pragma once

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
	virtual void run(state_machine& machine) = 0;
};
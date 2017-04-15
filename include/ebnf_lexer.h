#pragma once
#include "state_machine.h"
#include "ebnf.h"

#include <vector>
#include <algorithm>
#include <variant>
#include <string>

namespace ebnf
{
	class state : public abstract_state
	{
	protected:
		state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : line(line), iterator(current), tokens(tokens) {};
		std::string_view line;
		std::string_view::iterator iterator;
		std::vector<terminal>* tokens;
	};

	class state_decider : public state
	{
	public:
		state_decider(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		void run(state_machine& machine) override;
	};

	class quantifier_state : public state
	{
	public:
		quantifier_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '+')
				tokens->push_back(terminal::ONE_OR_MORE);
			else if (*iterator == '*')
				tokens->push_back(terminal::ZERO_OR_MORE);
			else if (*iterator == '?')
				tokens->push_back(terminal::ZERO_OR_ONE);

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class group_state : public state
	{
	public:
		group_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '(')
				tokens->push_back(terminal::BEGIN_GROUP);
			else if (*iterator == ')')
				tokens->push_back(terminal::END_GROUP);

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class alternation_state : public state
	{
	public:
		alternation_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '|') tokens->push_back(terminal::ALTERNATION_SIGN);
			else machine.exit();

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class end_of_rule_state : public state
	{
	public:
		end_of_rule_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator != '.') machine.exit();

			iterator++;
			tokens->push_back(terminal::END_OF_RULE);
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class identifier_state : public state
	{
	public:
		identifier_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			while (isalpha(*iterator)) iterator++;

			tokens->push_back(terminal::IDENTIFIER);

			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class string_state : public state
	{
	public:
		string_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			// Advance beyond first '
			iterator++;
			while (*iterator != '\'') iterator++;
			iterator++;

			tokens->push_back(terminal::STRING);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class assignment_state : public state
	{
	public:
		assignment_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator++ != ':') machine.exit();
			if (*iterator++ != ':') machine.exit();
			if (*iterator++ != '=') machine.exit();

			tokens->push_back(terminal::ASSIGNMENT);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class name_state : public state
	{
	public:
		name_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			while (iterator != line.end() && !isspace(*iterator)) iterator++;

			if (iterator == line.end())
			{
				machine.exit();
				return;
			}

			tokens->push_back(terminal::NAME);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class exception_state : public state
	{
	public:
		exception_state(std::string_view line, std::string_view::iterator current, std::vector<terminal>* tokens) : state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '-')
				tokens->push_back(terminal::EXCEPTION);

			iterator++;

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};
}
#pragma once
#include <string>
#include <regex>
#include <unordered_map>
#include <stack>
#include <variant>
#include "lexer.h"
#include "state_machine.h"

namespace lexer
{
	enum ebnf_terminal
	{
		NAME,
		ASSIGNMENT,
		STRING,
		IDENTIFIER,
		END_OF_RULE,
		ZERO_OR_MORE,
		ONE_OR_MORE,
		ZERO_OR_ONE,
		BEGIN_GROUP,
		END_GROUP,
		XOR_SIGN,
		EXCEPTION,
	};

	/////

	class ebnf_state : public abstract_state
	{
	protected:
		ebnf_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : line(line), iterator(current), tokens(tokens) {};
		std::string_view line;
		std::string_view::iterator iterator;
		std::vector<ebnf_terminal>* tokens;
	};

	class state_decider : public ebnf_state
	{
	public:
		state_decider(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		void run(state_machine& machine) override;
	};

	class quantifier_state : public ebnf_state
	{
	public:
		quantifier_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '+')
				tokens->push_back(ONE_OR_MORE);
			else if(*iterator == '*')
				tokens->push_back(ZERO_OR_MORE);
			else if(*iterator == '?')
				tokens->push_back(ZERO_OR_ONE);

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class group_state : public ebnf_state
	{
	public:
		group_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '(')
				tokens->push_back(BEGIN_GROUP);
			else if (*iterator == ')')
				tokens->push_back(END_GROUP);

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class alternation_state : public ebnf_state
	{
	public:
		alternation_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '|') tokens->push_back(XOR_SIGN);
			else machine.exit();

			iterator++;
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class end_of_rule_state : public ebnf_state
	{
	public:
		end_of_rule_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator != '.') machine.exit();

			iterator++;
			tokens->push_back(END_OF_RULE);
			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class identifier_state : public ebnf_state
	{
	public:
		identifier_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			while (isalpha(*iterator)) iterator++;

			tokens->push_back(IDENTIFIER);

			machine.transition(new state_decider(line, iterator, tokens));
		};

	};

	class string_state : public ebnf_state
	{
	public:
		string_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			// Advance beyond first '
			iterator++;
			while (*iterator != '\'') iterator++;
			iterator++;

			tokens->push_back(STRING);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class assignment_state : public ebnf_state
	{
	public:
		assignment_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator++ != ':') machine.exit();
			if (*iterator++ != ':') machine.exit();
			if (*iterator++ != '=') machine.exit();

			tokens->push_back(ASSIGNMENT);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class name_state : public ebnf_state
	{
	public:
		name_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			while (iterator != line.end() && !isspace(*iterator)) iterator++;

			if (iterator == line.end())
			{
				machine.exit();
				return;
			}

			tokens->push_back(NAME);

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};

	class exception_state : public ebnf_state
	{
	public:
		exception_state(std::string_view line, std::string_view::iterator current, std::vector<ebnf_terminal>* tokens) : ebnf_state(line, current, tokens) {};

		virtual void run(state_machine& machine)
		{
			if (*iterator == '-')
				tokens->push_back(EXCEPTION);

			iterator++;

			machine.transition(new state_decider(line, iterator, tokens));
		};
	};
	///////

	enum class ebnf_non_terminal
	{
		RULESET,
		EXPRESSION,
		ALTERNATION,
		GROUP,
		REPETITION,
		OPTIONAL_GROUP,
		EXCEPTION,
		CONCATENTATION,
	};

	class LL_table
	{
	public:
		LL_table()
		{
		
		}

		void parse(std::vector<ebnf_terminal> input)
		{

		}

	private:
		std::stack<std::variant<ebnf_terminal, ebnf_non_terminal>> stack;
	};

	class ebnf_node
	{
	public:
		virtual bool match(std::string_view content) = 0;
	};

	class ebnf_string : public ebnf_node
	{
	public:
		ebnf_string(std::string_view content) : value(content) {};

		bool match(std::string_view content) override
		{
			return value.compare(content) == 0;
		}

		std::string_view value;
	};

	class ebnf_identifier : public ebnf_node
	{
	public:
		ebnf_identifier(std::string_view content) : value(content) {};

		bool match(std::string_view content) override
		{
			return value.compare(content) == 0;
		}

		std::string_view value;
	};

	class ebnf_xor : public ebnf_node
	{
	public:
		ebnf_xor(const std::vector<ebnf_node*>& value);
		bool match(std::string_view content)
		{
			return false;
		}
	};

	class ebnf_zero_or_more : public ebnf_node
	{
	public:
		bool match(std::string_view content)
		{
			return false;
		}
	};

	class ebnf_rule : public ebnf_node
	{
	public:	

		std::string name;
	};

	class ebnf_ruleset : public ebnf_node
	{
	public:
	};

	/////////

	class ebnf_file
	{
	public:
		ebnf_file(std::string_view file);

	private:
		std::vector<ebnf_rule> rules;
	};

	namespace generator
	{
		language_lexer generate(const std::string& specification_location);
	};
}
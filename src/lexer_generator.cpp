#include "lexer_generator.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string_view>

namespace lexer
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

	////////////

	std::string_view trim(std::string_view content)
	{
		auto before = content.begin();
		while (before != content.end() && isspace(*before)) before++;
		std::string_view::reverse_iterator after = content.rbegin();
		while (after != content.rend() && isspace(*after)) after++;

		if (std::distance(before, after.base()) < 0) return "";

		return std::string_view(&*before, after.base() - before);
	}

	std::vector<std::string_view> trim_n(std::vector<std::string_view>&& content)
	{
		std::vector<std::string_view> res;
		std::transform(content.begin(), content.end(), std::back_inserter(res), [](auto string) {
			return trim(string);
		});
		return std::move(res);
	}

	std::vector<std::string_view> split(std::string_view content, char delim)
	{
		auto before = content.begin();
		auto after = content.begin();

		std::vector<std::string_view> splits;

		while (after != content.end())
		{
			while (after != content.end() && *after != delim) after++;

			splits.push_back(std::string_view(&*before, std::distance(before, after)));

			if (after == content.end()) break;

			after++;
			before = after;
		}

		return std::move(splits);
	}

	ebnf_xor::ebnf_xor(const std::vector<ebnf_node*>& content) {}

	//ebnf_rule::ebnf_rule(std::string_view line) 
	//{
	//	/*auto position = line.find_first_of("::=");

	//	name = trim(std::string_view(&*line.begin(), position));
	//	auto content = trim(std::string_view(&line.at(position + 3), line.end() - line.begin() - position - 3));
	//	rule = parse(content);*/
	//}

	//ebnf_node* ebnf_rule::parse(std::string_view line)
	//{
	//	// XOR
	//	auto xor_components = split(line, '|');
	//	if (xor_components.size() != 1)
	//	{
	//		std::vector<ebnf_node*> atoms;
	//		std::transform(xor_components.begin(), xor_components.end(), std::back_inserter(atoms), [this](auto x) {
	//			return parse(x);
	//		});
	//		return new ebnf_xor(std::move(atoms));
	//	}
	//}

	ebnf_file::ebnf_file(std::string_view file)
	{
		auto iterator = file.begin();

		auto string_rules = trim_n(split(file, '.'));
		//std::for_each(string_rules.begin(), string_rules.end() - 1, [this](auto x) {
		//	rules.push_back(ebnf_rule(x));
		//});
	}

	namespace generator
	{
		language_lexer generate(const std::string& specification)
		{
			// Parse rules
			std::ifstream in(specification, std::ios::in | std::ios::binary);
			if (!in) throw std::exception("Could not open file");

			std::string contents;
			in.seekg(0, std::ios::end);
			contents.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&contents[0], contents.size());
			in.close();
			std::string_view contents_view = contents;

			std::vector<ebnf_terminal> tokens;
			state_machine machine(new state_decider(contents_view, contents_view.begin(), &tokens));
			while (!machine.is_finished())
			{
				machine.current_state()->run(machine);
			}

			std::cout << tokens.size() << std::endl;
			/*lexical_definition* rules = new combination;
			language_lexer generated_lexer(rules);

			ebnf_file rule_file(contents);*/
			//return generated_lexer;
		}
	}
}
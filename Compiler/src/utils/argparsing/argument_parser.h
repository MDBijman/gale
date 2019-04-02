#pragma once
#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

std::optional<std::vector<std::string>>
parse_optional_list_option(const std::vector<std::string> &args, const std::string &option)
{
	std::vector<std::string> out;

	auto begin = std::find(args.begin(), args.end(), "-" + option);

	// The flag must be present
	if (begin == args.end()) { return std::nullopt; }

	auto end = std::find_if(begin + 1, args.end(), [](auto &s) { return s[0] == '-'; });

	if (
	  // The flag cannot be at the end
	  begin + 1 == args.end()
	  // There must be at least one file name
	  || end - begin == 1)
	{ return std::nullopt; }

	// We searched for the flag so the actual first input is one further
	begin++;

	// Gather list
	while (begin != end)
	{
		out.push_back(*begin);
		begin++;
	}

	return out;
}

std::vector<std::string> parse_list_option(const std::vector<std::string> &args,
					   const std::string &option)
{
	auto opt_res = parse_optional_list_option(args, option);

	if (!opt_res)
	{
		std::cerr << "Could not parse option " << option << "\n";
		std::exit(1);
	}

	return *opt_res;
}

std::optional<std::string> parse_optional_atom_option(const std::vector<std::string> &args,
						      const std::string &option)
{
	auto as_list = parse_optional_list_option(args, option);

	if (!as_list) return std::nullopt;
	if (as_list->size() != 1) return std::nullopt;

	return (*as_list)[0];
}

std::string parse_atom_option(const std::vector<std::string> &args, const std::string &option)
{
	auto opt_res = parse_optional_atom_option(args, option);

	if (!opt_res)
	{
		std::cerr << "Expected exactly one input for option " << option << "\n";
		std::exit(1);
	}

	return *opt_res;
}

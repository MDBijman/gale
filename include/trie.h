#pragma once
#include <variant>
#include <unordered_map>
#include <string>

using key_id = size_t;
namespace detail
{
	using trie_map = std::unordered_map<char, trie>;
}

class trie
{
public:
	bool is_leaf()
	{
		return std::holds_alternative<key_id>(content);
	}

	trie& get(char prefix)
	{
		if (!is_leaf()) return std::get<detail::trie_map>(content).at(prefix);
		else throw std::runtime_error("Leaf node");
	}

private:
	std::variant<
		key_id,
		detail::trie_map
	> content;
};
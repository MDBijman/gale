#pragma once
#include <vector>
#include <optional>
#include <string>

namespace fe::ext_ast
{
	// Node datatypes
	struct identifier
	{
		std::vector<std::string>   segments;
		std::optional<std::size_t> scope_distance;

		identifier without_first_segment() const
		{
			return identifier{ std::vector<std::string>(segments.begin() + 1, segments.end()), scope_distance };
		}

		identifier without_last_segment() const
		{
			return identifier{ std::vector<std::string>(segments.begin(), segments.end() - 1), scope_distance };
		}
	};

	inline bool operator==(const identifier& a, const identifier& b)
	{
		return (a.segments == b.segments) && (a.scope_distance == b.scope_distance);
	}

	struct boolean
	{
		bool value;
	};

	struct string
	{
		std::string value;
	};

	struct number
	{
		uint64_t value;
	};
}

namespace std
{
	template<> struct hash<fe::ext_ast::identifier>
	{
		size_t operator()(const fe::ext_ast::identifier& o) const
		{
			size_t h = 0;
			for (const auto& s : o.segments)
				h ^= hash<string>()(s);
			if (o.scope_distance)
				h ^= hash<size_t>()(o.scope_distance.value());
			return h;
		}
	};
}

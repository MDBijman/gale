#pragma once
#include <vector>
#include <optional>
#include <string>

namespace fe::ext_ast
{
	// Node datatypes
	struct identifier
	{
		identifier() {}
		identifier(std::vector<std::string> segs) : segments(segs) {}
		identifier(std::vector<std::string> segs, size_t sd) : segments(segs), scope_distance(sd) {}
		identifier(std::vector<std::string> segs, size_t sd, std::vector<size_t> offsets) : 
			segments(segs), scope_distance(sd), offsets(offsets) {}
		identifier(std::vector<std::string> segs, std::optional<size_t> sd, std::vector<size_t> offsets) : 
			segments(segs), scope_distance(sd), offsets(offsets) {}

		std::vector<std::string>   segments;
		std::optional<std::size_t> scope_distance;
		std::vector<size_t>        offsets;

		identifier without_first_segment() const
		{
			auto begin = offsets.size() == 0 ? offsets.begin() : offsets.begin() + 1;
			return identifier{ 
				std::vector<std::string>(segments.begin() + 1, segments.end()), 
				scope_distance, 
				std::vector<size_t>(begin, offsets.end()), 
			};
		}

		identifier without_last_segment() const
		{
			auto end = offsets.size() == 0 ? offsets.end() : offsets.end() - 1;
			return identifier(
				std::vector<std::string>(segments.begin(), segments.end() - 1),
				scope_distance,
				std::vector<size_t>(offsets.begin(), end) 
			);
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

	/*
	* The index of a piece of data in a data store. Each node type that contains data has its own data type
	* and corresponding data store. If a node type has no data, then the index will be the maximum value,
	* i.e. std::numeric_limits<data_index>::max().
	*/
	using data_index = size_t;
	using type_index = size_t;
	using scope_index = size_t;
	using node_id = size_t;
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

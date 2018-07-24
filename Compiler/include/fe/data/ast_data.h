#pragma once
#include <vector>
#include <optional>
#include <string>

namespace fe::types { struct type; }

namespace fe::ext_ast
{
	using name = std::string;
	using module_name = std::vector<std::string>;

	struct identifier
	{
		name full;
		module_name segments;
		std::optional<std::size_t> scope_distance;
		std::vector<size_t> offsets;
		uint32_t unique_id = 0;

		identifier without_first_segment() const
		{
			auto begin = offsets.size() == 0 ? offsets.begin() : offsets.begin() + 1;
			return identifier{
				full,
				std::vector<std::string>(segments.begin() + 1, segments.end()),
				scope_distance,
				std::vector<size_t>(begin, offsets.end()),
			};
		}

		identifier without_last_segment() const
		{
			auto end = offsets.size() == 0 ? offsets.end() : offsets.end() - 1;
			return identifier{
				full,
				std::vector<std::string>(segments.begin(), segments.end() - 1),
				scope_distance,
				std::vector<size_t>(offsets.begin(), end)
			};
		}

		std::vector<std::string> copy_name() const
		{
			std::vector<std::string> out;
			for (auto seg : segments) out.push_back(std::string(seg));
			return out;
		}

		operator std::string() const
		{
			return std::string(full);
		}
	};

	inline bool operator==(const identifier& a, const identifier& b)
	{
		return (a.segments == b.segments) && (a.scope_distance == b.scope_distance);
	}
}

namespace std
{
	template<> struct hash<fe::ext_ast::module_name>
	{
		size_t operator()(const fe::ext_ast::module_name& n) const
		{
			size_t h = 0;
			for (const auto& s : n)
				h ^= hash<string>()(s);
			return h;
		}
	};

	template<> struct hash<fe::ext_ast::identifier>
	{
		size_t operator()(const fe::ext_ast::identifier& o) const
		{
			size_t h = 0;
			for (const auto& s : o.segments)
				h ^= hash<string>()(s);
			if (o.scope_distance)
				h ^= hash<size_t>()(o.scope_distance.value());
			for (const auto& s : o.offsets)
				h ^= hash<size_t>()(s);
			return h;
		}
	};
}

namespace fe::core_ast
{
	struct identifier
	{
		identifier() : unique_id(0) {}
		identifier(uint32_t id) : unique_id(id) {}
		identifier(std::string module, uint32_t id) : modules({ module }), unique_id(id) {}
		identifier(std::vector<std::string> ms, uint32_t id) : modules(ms), unique_id(id) {}

		std::vector<std::string> modules;
		uint32_t unique_id;

		operator std::string() const
		{
			std::string o;
			for (int i = 0; i < modules.size(); i++)
				o += std::string(modules.at(i)) + ".";
			o += std::to_string(unique_id);
			return o;
		}
	};

	inline bool operator==(const identifier& a, const identifier& b)
	{
		return (a.modules == b.modules) && (a.unique_id == b.unique_id);
	}
}

namespace std
{
	template<> struct hash<fe::core_ast::identifier>
	{
		size_t operator()(const fe::core_ast::identifier& o) const
		{
			size_t h = 0;
			for (const auto& s : o.modules)
				h ^= hash<string_view>()(s);
			h ^= hash<uint32_t>()(o.unique_id);
			return h;
		}
	};
}

namespace fe
{
	struct plain_identifier
	{
		std::string full;
	};

	struct boolean
	{
		bool value;
	};

	struct string
	{
		std::string value;
	};

	enum class number_type { UI32, I32, UI64, I64 };

	struct number
	{
		long long int value;
		number_type type;
	};

	using scope_index = uint32_t;
	constexpr scope_index no_scope = std::numeric_limits<uint32_t>::max();

	using node_id = uint32_t;
	constexpr node_id no_node = std::numeric_limits<uint32_t>::max();

	using data_index = uint32_t;
	constexpr data_index no_data = std::numeric_limits<uint32_t>::max();

	using children_id = uint32_t;
	constexpr children_id no_children = std::numeric_limits<uint32_t>::max();
}

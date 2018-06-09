#pragma once
#include <vector>
#include <optional>
#include <string>

namespace fe::types { struct type; }

namespace fe::ext_ast
{

	using name = std::string_view;
	using module_name = std::vector<std::string_view>;

	struct identifier
	{
		name full;
		module_name segments;
		std::optional<std::size_t> scope_distance;
		std::vector<size_t> offsets;

		identifier without_first_segment() const
		{
			auto begin = offsets.size() == 0 ? offsets.begin() : offsets.begin() + 1;
			return identifier{
				full,
				std::vector<std::string_view>(segments.begin() + 1, segments.end()),
				scope_distance,
				std::vector<size_t>(begin, offsets.end()),
			};
		}

		identifier without_last_segment() const
		{
			auto end = offsets.size() == 0 ? offsets.end() : offsets.end() - 1;
			return identifier{
				full,
				std::vector<std::string_view>(segments.begin(), segments.end() - 1),
				scope_distance,
				std::vector<size_t>(offsets.begin(), end)
			};
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
				h ^= hash<string_view>()(s);
			return h;
		}
	};

	template<> struct hash<fe::ext_ast::identifier>
	{
		size_t operator()(const fe::ext_ast::identifier& o) const
		{
			size_t h = 0;
			for (const auto& s : o.segments)
				h ^= hash<string_view>()(s);
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
		identifier() : scope_distance(0) {}
		identifier(std::string_view name) : variable_name(name), scope_distance(0) {}
		identifier(std::vector<std::string_view> modules, std::string_view name, size_t sd, std::vector<size_t> offsets) :
			modules(modules), variable_name(name), scope_distance(sd), offsets(offsets) {}

		std::vector<std::string_view>   modules;
		std::string_view                variable_name;
		std::size_t                     scope_distance;
		std::vector<size_t>             offsets;

		operator std::string() const
		{
			std::string o;
			for (int i = 0; i < modules.size(); i++)
				o += std::string(modules.at(i)) + ".";
			o += variable_name;
			return o;
		}
	};

	inline bool operator==(const identifier& a, const identifier& b)
	{
		return (a.modules == b.modules) && (a.variable_name == b.variable_name)
			&& (a.scope_distance == b.scope_distance) && (a.offsets == b.offsets);
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
			h ^= hash<string_view>()(o.variable_name);
			h ^= hash<size_t>()(o.scope_distance);
			for (const auto& s : o.offsets)
				h ^= hash<size_t>()(s);
			return h;
		}
	};
}

namespace fe
{
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

	using scope_index = size_t;
	using node_id = size_t;
	using data_index = size_t;
}

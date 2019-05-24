#pragma once
#include <vector>
#include <optional>
#include <string>
#include <assert.h>
#include <limits>

namespace fe::types { struct type; }

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

	enum class number_type { UI8, I8, UI16, I16, UI32, I32, UI64, I64 };

	constexpr uint8_t number_size(number_type t)
	{
		switch (t)
		{
		case number_type::UI8: return 1;
		case number_type::I8: return 1;
		case number_type::UI16: return 2;
		case number_type::I16: return 2;
		case number_type::UI32: return 4;
		case number_type::I32: return 4;
		case number_type::UI64: return 8;
		case number_type::I64: return 8;
		default: throw std::runtime_error("Invalid number type"); return 0;
		}
	}

	struct number
	{
		long long int value;
		number_type type;
	};

	using scope_index = uint32_t;
	constexpr scope_index no_scope = std::numeric_limits<uint32_t>::max();

	using node_id = uint32_t;
	constexpr node_id no_node = std::numeric_limits<uint32_t>::max();

	using data_index_t = uint32_t;
	constexpr data_index_t no_data = std::numeric_limits<uint32_t>::max();

	using children_id = uint32_t;
	constexpr children_id no_children = std::numeric_limits<uint32_t>::max();
}

namespace fe::ext_ast
{
	using name = std::string;
	using module_name = std::vector<std::string>;

	struct identifier
	{
		std::string name;

		std::vector<std::string> module_path;

		std::string full;

		std::optional<std::size_t> scope_distance;

		node_id type_node = no_node;

		bool is_parameter = false;

		// The index in function of an identifier is equal to the index of the identifier in the stack of local variables.
		uint32_t index_in_function = std::numeric_limits<uint32_t>::max();

		std::optional<std::pair<uint32_t, int32_t>> referenced_stack_label;

		operator std::string() const
		{
			return std::string(full);
		}

		std::vector<std::string> full_path() const
		{
			auto path = module_path;
			path.push_back(name);
			return path;
		}

		void recompute_full()
		{
			full = "";
			for(auto& segment : module_path)
			{
				full += segment + ".";
			}
			full += name;
		}
	};

	inline bool operator==(const identifier& a, const identifier& b)
	{
		return (a.full == b.full) && (a.scope_distance == b.scope_distance);
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
			h ^= hash<string>()(o.full);
			if (o.scope_distance)
				h ^= hash<size_t>()(o.scope_distance.value());
			return h;
		}
	};
}

namespace fe::core_ast
{
	struct label
	{
		uint32_t id;
	};

	struct stack_label
	{
		uint32_t id;
	};

	struct relative_offset
	{
		uint32_t label_id;
		int32_t offset;
	};

	inline bool operator==(const label& l, const label& r)
	{
		return l.id == r.id;
	}

	struct var_data
	{
		uint32_t offset = 0, size = 0;
	};

	struct size
	{
		size_t val = std::numeric_limits<size_t>::max();

		bool valid()
		{
			return val != std::numeric_limits<size_t>::max();
		}
	};

	struct return_data
	{
		return_data() : in_size(0), frame_size(0), out_size(0) {}
		return_data(size_t in_size, size_t frame_size, size_t out_size) : in_size(in_size), frame_size(frame_size), out_size(out_size) {}
		size_t in_size, frame_size, out_size;
	};

	struct function_data
	{
		function_data() {}
		function_data(std::string name, size_t in, size_t out, uint32_t var_size) : name(name), in_size(in), out_size(out), locals_size(var_size) {}
		std::string name;
		size_t in_size, out_size;
		uint32_t locals_size;
	};

	struct function_call_data
	{
		function_call_data() {}
		function_call_data(const std::string& name, uint32_t in, uint32_t out) : name(name), out_size(out), in_size(in) {}
		std::string name;
		uint32_t out_size, in_size;
	};

	struct function_ref_data
	{
		function_ref_data() {}
		function_ref_data(const std::string& name) : name(name) {}

		std::string name;
	};
}

namespace std
{
	template<> struct hash<fe::core_ast::label>
	{
		size_t operator()(const fe::core_ast::label& o) const
		{
			return std::hash<uint32_t>()(o.id);
		}
	};
}
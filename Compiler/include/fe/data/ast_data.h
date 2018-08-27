#pragma once
#include <vector>
#include <optional>
#include <string>

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

		node_id type_node = no_node;

		// The index in function of an identifier is equal to the index of the register that is used to store the address
		// The address of the nth declared variable in a function is stored in the nth register
		uint32_t index_in_function = static_cast<uint32_t>(-1);
		int32_t offset_from_fp = std::numeric_limits<int32_t>::max();

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

		identifier root_identifier() const
		{
			return identifier{
				segments[0],
				{ segments[0] },
				scope_distance,
				{}
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

		std::string mangle() const
		{
			std::string res;
			int64_t segments = this->segments.size(), offsets = this->offsets.size();

			for (int i = 0; i < segments - offsets - 2; i++)
				res += this->segments[i] + ".";

			if(this->segments.size() > this->offsets.size() + 1)
				res += this->segments[segments - offsets - 2] + "@";
			res += this->segments[segments - offsets - 1];
			return res;
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
	struct label
	{
		uint32_t id;
	};

	inline bool operator==(const label& l, const label& r)
	{
		return l.id == r.id;
	}

	struct size
	{
		size_t val;
	};

	struct return_data
	{
		size_t in_size, out_size;
	};

	struct move_data
	{
		move_data() {}
		enum class dst_type
		{
			// register
			REG,
			// stack pointer register
			SP,
			// instruction pointer register
			IP,
			// result register
			RES,

			// stack 
			RELATIVE_TO_SP,
			RELATIVE_TO_FP,

			// register containing location
			LOC,
			// register containing location in 32 msbits and offset in 32 lsbits
			LOC_WITH_OFFSET
		};
		enum class src_type
		{
			// register
			REG,
			// stack pointer register
			SP,
			// instruction pointer register
			IP,
			// result register
			RES,

			// stack 
			RELATIVE_TO_SP,
			RELATIVE_TO_FP,

			// register containing location 
			LOC,
			// register containing location in 32 msbits and offset in 32 lsbits
			LOC_WITH_OFFSET,

			// literal value
			LIT
		};
		dst_type to_t;
		src_type from_t;

		/*
		* Contains the value indicating the src/dest value.
		* If the type is REG, contains the register id.
		* If the type is STACK, contains the offset from SP.
		* If the type is LOC, contains the register id of the register containing the location.
		* If the type is LIT, contains the literal value.
		* If the type is LOC_WITH_OFFSET, contains the loc in the 32 msbits, and the offset in32 lsbits
		* Otherwise this value is unused.
		*/
		int64_t from, to;

		/*
		* The size of the move to be performed, in bytes.
		*/
		uint32_t size;
	};

	struct function_data
	{
		function_data() {}
		std::string name;
		size_t in_size, out_size;
		uint32_t label;
	};

	struct function_call_data
	{
		function_call_data() {}
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
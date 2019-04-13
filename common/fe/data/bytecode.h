#pragma once
#include "fe/vm/runtime_info.h"
#include <algorithm>
#include <array>
#include <assert.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fe::vm
{
	enum class op_kind : uint8_t
	{
		/*
		 * Other
		 */

		NOP = 0,
		EXIT, // Dealloc remaining space? Return code?
		ERR,
		// temporary label with id
		LBL_UI32,

		/*
		 * Arithmetic
		 */

		// reg[b0] <- reg[b1] + reg[b2]
		ADD_R64_R64_R64,
		ADD_R64_R64_UI8,
		// reg[b0] <- reg[b1] - reg[b2]
		SUB_R64_R64_R64,
		SUB_R64_R64_UI8,
		// reg[b0] <- reg[b1] * reg[b2]
		MUL_R64_R64_R64,
		// reg[b0] <- reg[b1] / reg[b2]
		DIV_R64_R64_R64,
		// reg[b0] <- reg[b1] % reg[b2]
		MOD_R64_R64_R64,

		/*
		 * Logic
		 */

		// if reg[b1] > reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GT_R8_R64_R64,
		GT_R8_R8_R8,
		// if reg[b1] >= reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GTE_R8_R64_R64,
		GTE_R8_R8_R8,
		// if reg[b1] < reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		LT_R8_R64_R64,
		LT_R8_R8_R8,
		// if reg[b1] <= reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		LTE_R8_R64_R64,
		LTE_R8_R8_R8,
		// if reg[b1] == reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		EQ_R8_R64_R64,
		EQ_R8_R8_R8,
		// if reg[b1] != reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		NEQ_R8_R64_R64,
		NEQ_R8_R8_R8,
		// reg[b0] <- reg[b1] & reg[b2]
		AND_R64_R64_R64,
		AND_R8_R8_R8,
		AND_R8_R8_UI8,
		// reg[b0] <- reg[b1] | reg[b2]
		OR_R64_R64_R64,
		OR_R8_R8_R8,
		// reg[b0] <- reg[b1] ^ b2
		XOR_R8_R8_UI8,

		/*
		 * Data
		 */

		// Moves unsigned literal into destination registers
		MV_REG_UI8,
		MV_REG_UI16,
		MV_REG_UI32,
		MV_REG_UI64,

		// Moves literal into destination registers
		MV_REG_I8,
		MV_REG_I16,
		MV_REG_I32,
		MV_REG_I64,

		// Moves N bytes from source registers to destination registers
		MV_RN_RN,
		// Moves N bytes from source location to destination registers
		MV_RN_LN,

		/*
		 * Control
		 */

		// jump relative: ip += b0
		JMPR_I32,
		// jump relative not zero: if reg[b0] != 0 { ip += b1 } else { ip++ }
		JRNZ_REG_I32,
		// jump relative zero: if reg[b0] == 0 { ip += b1 } else { ip++ }
		JRZ_REG_I32,

		// Call(target ip address, first argument register, argument count, first return
		// register)
		CALL_UI64_UI8_UI8_UI8,
		CALL_NATIVE_UI64_UI8_UI8,

		// Used at the start of a new scope to allocate registers
		ALLOC_UI8,

		// Ret(argument count, additional frame size, first return register, return register
		// count)
		RET_UI8_UI8_UI8_UI8,
	};

	// Returns the byte representation of the given kind
	uint8_t op_to_byte(op_kind);
	// Returns the kind (enum) representation of the given byte
	op_kind byte_to_op(uint8_t);
	// Returns a string representation of the given kind
	std::string op_to_string(op_kind);
	// Returns a kind (enum) parsed from the given string
	op_kind string_to_op(const std::string &);

	// Returns max uint8_t value on error
	constexpr uint8_t op_size(op_kind o)
	{
		// Cannot be put in cpp file because of constexpr
		switch (o)
		{
		case op_kind::NOP: return 1;
		case op_kind::ADD_R64_R64_R64: return 4;
		case op_kind::ADD_R64_R64_UI8: return 4;
		case op_kind::SUB_R64_R64_R64: return 4;
		case op_kind::SUB_R64_R64_UI8: return 4;
		case op_kind::MUL_R64_R64_R64: return 4;
		case op_kind::DIV_R64_R64_R64: return 4;
		case op_kind::MOD_R64_R64_R64: return 4;
		case op_kind::GT_R8_R64_R64: return 4;
		case op_kind::GT_R8_R8_R8: return 4;
		case op_kind::GTE_R8_R64_R64: return 4;
		case op_kind::GTE_R8_R8_R8: return 4;
		case op_kind::LT_R8_R64_R64: return 4;
		case op_kind::LT_R8_R8_R8: return 4;
		case op_kind::LTE_R8_R64_R64: return 4;
		case op_kind::LTE_R8_R8_R8: return 4;
		case op_kind::EQ_R8_R64_R64: return 4;
		case op_kind::EQ_R8_R8_R8: return 4;
		case op_kind::NEQ_R8_R64_R64: return 4;
		case op_kind::NEQ_R8_R8_R8: return 4;
		case op_kind::AND_R64_R64_R64: return 4;
		case op_kind::AND_R8_R8_R8: return 4;
		case op_kind::AND_R8_R8_UI8: return 4;
		case op_kind::OR_R64_R64_R64: return 4;
		case op_kind::OR_R8_R8_R8: return 4;
		case op_kind::XOR_R8_R8_UI8: return 4;
		case op_kind::MV_REG_UI8: return 3;
		case op_kind::MV_REG_UI16: return 4;
		case op_kind::MV_REG_UI32: return 6;
		case op_kind::MV_REG_UI64: return 10;
		case op_kind::MV_REG_I8: return 3;
		case op_kind::MV_REG_I16: return 4;
		case op_kind::MV_REG_I32: return 6;
		case op_kind::MV_REG_I64: return 10;
		case op_kind::MV_RN_RN: return 4;
		case op_kind::MV_RN_LN: return 4;
		case op_kind::LBL_UI32: return 5;
		case op_kind::JMPR_I32: return 5;
		case op_kind::JRNZ_REG_I32: return 6;
		case op_kind::JRZ_REG_I32: return 6;
		case op_kind::CALL_UI64_UI8_UI8_UI8: return 12;
		case op_kind::CALL_NATIVE_UI64_UI8_UI8: return 11;
		case op_kind::ALLOC_UI8: return 2;
		case op_kind::RET_UI8_UI8_UI8_UI8: return 5;
		case op_kind::EXIT: return 1;
		default: throw std::runtime_error("Unknown Op");
		}
	}

	// Compile time op size struct
	template <op_kind Op> struct ct_op_size
	{
		static constexpr uint8_t value = op_size(Op);
	};

	// A byte is, as the name suggests, a single byte of a bytecode object
	struct byte
	{
		byte() : val(0)
		{
		}
		byte(uint8_t v) : val(v)
		{
		}
		uint8_t val;
	};
	inline byte operator+(const byte &a, const byte &b)
	{
		return byte(a.val + b.val);
	}
	inline byte operator-(const byte &a, const byte &b)
	{
		return byte(a.val - b.val);
	}
	inline byte operator*(const byte &a, const byte &b)
	{
		return byte(a.val * b.val);
	}
	inline byte operator%(const byte &a, const byte &b)
	{
		return byte(a.val % b.val);
	}
	inline bool operator>(const byte &a, const byte &b)
	{
		return (a.val > b.val);
	}

	// A reg is an index corresponding to a register
	struct reg
	{
		reg(uint8_t v) : val(v)
		{
		}
		uint8_t val;
	};
	inline bool operator==(const reg &a, const reg &b)
	{
		return a.val == b.val;
	}

	// Return true if the op writes to the given register
	bool writes_to(const byte *op, reg r);
	// Return true if the op reads from the given register
	bool reads_from(const byte *op, reg r);

	template <size_t C> using bytes = std::array<byte, C>;

	template <int C> bytes<C> make_nops()
	{
		bytes<C> out;
		for (int i = 0; i < C; i++) out[i] = op_to_byte(op_kind::NOP);
		return out;
	}

	// Helper functions for creating bytecode literals
	bytes<8> make_i64(int64_t);
	int64_t read_i64(const uint8_t *);
	int64_t read_i64(bytes<8>);

	bytes<8> make_ui64(uint64_t);
	uint64_t read_ui64(const uint8_t *);
	uint64_t read_ui64(bytes<8>);

	bytes<4> make_i32(int32_t);
	int32_t read_i32(const uint8_t *);
	int32_t read_i32(bytes<4>);

	bytes<4> make_ui32(uint32_t);
	uint32_t read_ui32(const uint8_t *);
	uint32_t read_ui32(bytes<4>);

	bytes<2> make_ui16(uint16_t);
	uint16_t read_ui16(const uint8_t *);
	uint16_t read_ui16(bytes<2>);

	bytes<2> make_i16(int16_t);
	int16_t read_i16(const uint8_t *);
	int16_t read_i16(bytes<2>);

	bytes<1> make_ui8(uint8_t);
	uint8_t read_ui8(const uint8_t *);
	uint8_t read_ui8(bytes<1>);

	bytes<1> make_i8(int8_t);
	int8_t read_i8(const uint8_t *);
	int8_t read_i8(bytes<1>);

	// Operator construction methods
	bytes<1> make_nop();
	bytes<4> make_add_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_add_r64_r64_ui8(reg dest, reg a, byte b);
	bytes<4> make_sub_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_sub_r64_r64_ui8(reg dest, reg a, byte b);
	bytes<4> make_mul_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_div_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_mod_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_gt_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_gt_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_gte_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_gte_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_lt_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_lt_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_lte_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_lte_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_eq_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_eq_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_neq_r8_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_neq_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_and_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_and_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_and_r8_r8_ui8(reg dest, reg a, reg b);
	bytes<4> make_or_r64_r64_r64(reg dest, reg a, reg b);
	bytes<4> make_or_r8_r8_r8(reg dest, reg a, reg b);
	bytes<4> make_xor_r8_r8_ui8(reg dest, reg a, int8_t b);
	bytes<3> make_mv_reg_ui8(reg dest, uint8_t a);
	bytes<4> make_mv_reg_ui16(reg dest, uint16_t a);
	bytes<6> make_mv_reg_ui32(reg dest, uint32_t a);
	bytes<10> make_mv_reg_ui64(reg dest, uint64_t a);
	bytes<3> make_mv_reg_i8(reg dest, int8_t a);
	bytes<4> make_mv_reg_i16(reg dest, int16_t a);
	bytes<6> make_mv_reg_i32(reg dest, int32_t a);
	bytes<10> make_mv_reg_i64(reg dest, int64_t a);
	bytes<3> make_mv_reg_reg(uint8_t bytes, reg dest, reg a);
	bytes<4> make_mv_rn_rn(byte count, reg dest, reg src);
	bytes<4> make_mv_rn_ln(byte count, reg dest, reg src);
	bytes<2> make_alloc_ui8(uint8_t size);
	bytes<12> make_call_ui64_ui8_ui8_ui8(uint64_t ip, uint8_t reg, uint8_t regc,
					     uint8_t ret_reg);
	bytes<11> make_call_native_ui64_ui8_ui8(uint64_t ip, uint8_t in_reg, uint8_t out_reg);
	bytes<5> make_ret(byte paramc, byte fs, byte first_reg, byte retc);
	bytes<5> make_jmpr_i32(int32_t offset);
	bytes<6> make_jrnz_i32(reg a, int32_t offset);
	bytes<6> make_jrz_i32(reg a, int32_t offset);
	bytes<5> make_lbl(uint32_t id);
	bytes<1> make_exit();

	// A far_lbl is used to refer to an instruction and the bytecode chunk it is a part of
	struct far_lbl
	{
		far_lbl() : chunk_id(0), ip(0)
		{
		}
		far_lbl(uint64_t chunk, uint64_t ip) : chunk_id(chunk), ip(ip)
		{
		}
		uint64_t chunk_id;
		uint64_t ip;
		uint64_t make_ip()
		{
			return (chunk_id << 32) | ip;
		}
	};

	// A near_lbl is used to refer to an instruction within a single bytecode
	struct near_lbl
	{
		near_lbl(uint64_t i) : ip(i)
		{
		}
		uint64_t ip;
	};
	inline near_lbl operator+(const near_lbl &a, const near_lbl &b)
	{
		return near_lbl(a.ip + b.ip);
	}
	inline near_lbl operator-(const near_lbl &a, const near_lbl &b)
	{
		return near_lbl(a.ip - b.ip);
	}

	// A bytecode object is a linear vector of instructions that can be executed.
	class bytecode
	{
	      public:
		class iterator
		{
			friend class bytecode;

			std::vector<byte> &data;
			uint64_t i;

			iterator(std::vector<byte> &c);
			iterator(std::vector<byte> &c, uint64_t i);

		      public:
			using value_type = byte *;
			using reference = value_type;
			using pointer = byte **;
			using iterator_category = std::input_iterator_tag;
			using difference_type = int;

			iterator(const iterator &o);
			iterator &operator=(const iterator &o);
			iterator add_unsafe(uint64_t offset);
			// postfix
			iterator operator++(int);
			// prefix
			iterator &operator++();
			bool operator==(const iterator &o);
			bool operator!=(const iterator &o);
			byte *operator*();
			byte *operator->();
		};

	      private:
		std::vector<byte> instructions;

	      public:
		bytecode();
		bytecode(std::vector<byte> bs);

		// Adds the bytes to this bytecode at the given address
		template <size_t C> near_lbl add_instruction(near_lbl l, bytes<C> in)
		{
			for (int i = 0; i < C; i++)
				instructions.insert(instructions.begin() + l.ip + i, in[i]);
			return l;
		}

		// Adds the bytes to the end of this bytecode, returning the address of the first
		// byte
		template <size_t C> std::pair<near_lbl, uint32_t> add_instruction(bytes<C> in)
		{
			near_lbl l(instructions.size());
			for (int i = 0; i < C; i++) instructions.push_back(in[i]);
			return std::make_pair(l, static_cast<uint32_t>(in.size()));
		}

		// Adds the vector of bytes to the end of this bytecode, returning the address of
		// the first byte
		template <size_t... Cs>
		std::pair<near_lbl, uint32_t> add_instructions(bytes<Cs>... in)
		{
			near_lbl l(instructions.size());
			(add_instruction(in), ...);
			return std::make_pair(l, (Cs + ... + 0));
		}

		const byte *get_instruction(near_lbl l) const;

		// Returns the C bytes starting at the given address, padded with op_kind::ERR bytes
		template <size_t C> bytes<C> get_instruction(near_lbl l) const
		{
			bytes<C> res;
			for (int i = 0; i < C; i++)
				res[i] = i + l.ip < instructions.size() ? instructions[l.ip + i]
									: op_to_byte(op_kind::ERR);
			return res;
		}

		// Sets the instruction at the given address to the new bytes
		template <size_t C> void set_instruction(near_lbl l, bytes<C> b)
		{
			for (int i = 0; i < C; i++) instructions[l.ip + i] = b[i];
		}

		byte *operator[](uint64_t index);

		void append(const bytecode &other);

		// Returns true if the given address maps to an instruction
		bool has_instruction(near_lbl) const;

		operator std::string() const;

		size_t size() const;

		std::vector<byte> &data();
		const std::vector<byte> &data() const;

		iterator begin();
		iterator end();
	};

	// A helper class for building a bytecode
	class bytecode_builder
	{
		bytecode bc;

	      public:
		// Adds the bytes to the end of the bytecode
		template <long unsigned int C> bytecode_builder &add(bytes<C> in)
		{
			bc.add_instruction(in);
			return *this;
		}

		// Adds the vector of bytes to the end of the bytecode
		template <long unsigned int... Cs> bytecode_builder &add(bytes<Cs>... in)
		{
			bc.add_instructions(in...);
			return *this;
		}

		bytecode build();
	};

	using name = std::string;
	using symbols = std::unordered_map<uint32_t, name>;

	// A bytecode object/native function with a name, referenced in other bytecode by the name
	// A function object also contains a map of external functions referenced by name
	class function
	{
		name signature;
		std::variant<bytecode, native_function_id> code;
		symbols externals;

	      public:
		function(name n, bytecode c, symbols s);
		function(name n, bytecode c);
		function(name n, native_function_id c, symbols s);
		function(name n, native_function_id c);
		function();

		const name &get_name() const;
		symbols &get_symbols();

		bool is_bytecode() const;
		bytecode &get_bytecode();
		const bytecode &get_bytecode() const;

		bool is_native() const;
		native_function_id get_native_function_id() const;
	};

	// A function id is a unique id for a function
	using function_id = uint16_t;

	// A module is a set of functions
	class module
	{
		std::vector<function> code;

	      public:
		module()
		{
		}

		function_id add_function(function);
		function &get_function(function_id);
		function &get_function(name);
		size_t function_count() const;

		void insert_padding(far_lbl loc, uint8_t size);

		std::vector<function> &get_code();
		const std::vector<function> &get_code() const;

		template <int C> bytes<C> operator[](far_lbl l)
		{
			return code.at(l.chunk_id).get_bytecode().get_instruction<C>(l.ip);
		}

		std::string to_string();
	};

	// An executable is a single monolithic bytecode object combined with a set of native
	// functions.
	class executable
	{
	      public:
		bytecode code;

		executable(bytecode code);

		template <int C> bytes<C> get_instruction(uint64_t loc)
		{
			return code.get_instruction<C>(loc);
		}

		std::string to_string();

		bytecode::iterator begin();
		bytecode::iterator end();
		byte *operator[](uint64_t i);

		size_t byte_length() const;
	};

	// A direct threaded executable is a platform dependent executable where op_kinds in the
	// bytecode are replaced with offsets in the interpreter code.
	class direct_threaded_executable
	{
	      public:
		direct_threaded_executable(bytecode code);

		bytecode code;
	};
} // namespace fe::vm

namespace std
{
	template <> struct hash<fe::vm::reg>
	{
		size_t operator()(const fe::vm::reg &r) const
		{
			return std::hash<uint64_t>()(r.val);
		}
	};
} // namespace std

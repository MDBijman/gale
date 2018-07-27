#pragma once
#include <vector>
#include <array>
#include <variant>

namespace fe::vm
{
	enum class op_kind : uint8_t
	{
		NOP = 0,

		/*
		* Arithmetic
		*/

		// reg[b0] <- reg[b1] + reg[b2]
		ADD_REG_REG_REG,
		// reg[b0] <- reg[b1] - reg[b2]
		SUB_REG_REG_REG,
		// reg[b0] <- reg[b1] * reg[b2]
		MUL_REG_REG_REG,
		// reg[b0] <- reg[b1] / reg[b2]
		DIV_REG_REG_REG,
		// reg[b0] <- reg[b1] % reg[b2]
		MOD_REG_REG_REG,
		
		/*
		* Logic
		*/

		// if reg[b1] > reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GT_REG_REG_REG,
		// if reg[b1] >= reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GTE_REG_REG_REG,
		// if reg[b1] == reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		EQ_REG_REG_REG,
		// if reg[b1] != reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		NEQ_REG_REG_REG,
		// if reg[b1] != 0 && reg[b2] != 0 { reg[b0] <- 1 } else { reg[b0] <- 0 }
		AND_REG_REG_REG,
		// if reg[b1] != 0 || reg[b2] != 0 { reg[b0] <- 1 } else { reg[b0] <- 0 }
		OR_REG_REG_REG,

		/*
		* Control
		*/

		// reg[b0] <- *reg[b1]
		LOAD64_REG_REG,
		// reg[b0] <- sp
		LOAD_SP_REG,
		// reg[b0] <- b1
		MV_REG_UI8,
		// nop, use to jump
		LBL,
		// stack[esp] <- reg[b0], esp++
		PUSH8_REG,
		PUSH16_REG,
		PUSH32_REG,
		PUSH64_REG,
		// reg[b0] <- stack[esp - 1], esp--
		POP8_REG,
		POP16_REG,
		POP32_REG,
		POP64_REG,
		// ip <- reg[b0]
		JMP_REG,
		// if reg[b0] != 0 { ip <- reg[b1] } else { ip++ }
		JNZ_REG_REG,
		// if reg[b0] == 0 { ip <- reg[b1] } else { ip++ }
		JZ_REG_REG,
		// push bp, push ip, bp <- reg[b0], ip <- reg[b1]
		CALL_REG_REG,
		// reg[x] <- pop, ip <- reg[x]
		RET_UI8,
	};

	uint8_t op_to_byte(op_kind);
	op_kind byte_to_op(uint8_t);

	struct byte
	{
		byte() : val(0) {}
		byte(uint8_t v) : val(v) {}
		uint8_t val;
	};
	inline byte operator+(const byte& a, const byte& b) { return byte(a.val + b.val); }
	inline byte operator-(const byte& a, const byte& b) { return byte(a.val - b.val); }
	inline byte operator*(const byte& a, const byte& b) { return byte(a.val * b.val); }
	inline byte operator%(const byte& a, const byte& b) { return byte(a.val % b.val); }
	inline bool operator>(const byte& a, const byte& b) { return (a.val > b.val); }

	struct reg
	{
		reg(uint8_t v) : val(v) {}
		uint8_t val;
	};
	inline bool operator==(const reg& a, const reg& b) { return a.val == b.val; }

	// Single byte
	using single_byte = std::array<byte, 1>;
	// Two bytes
	using double_byte = std::array<byte, 2>;
	// Three bytes
	using triple_byte = std::array<byte, 3>;
	// Four bytes
	using quad_byte = std::array<byte, 4>;

	using any_byte = std::variant<single_byte, double_byte, triple_byte, quad_byte>;

	// Operator construction methods
	single_byte make_nop();
	quad_byte make_add(reg dest, reg a, reg b);
	quad_byte make_sub(reg dest, reg a, reg b);
	quad_byte make_mul(reg dest, reg a, reg b);
	quad_byte make_div(reg dest, reg a, reg b);
	quad_byte make_mod(reg dest, reg a, reg b);
	quad_byte make_and(reg dest, reg a, reg b);
	quad_byte make_or(reg dest, reg a, reg b);
	quad_byte make_gt(reg dest, reg a, reg b);
	quad_byte make_gte(reg dest, reg a, reg b);
	quad_byte make_eq(reg dest, reg a, reg b);
	quad_byte make_neq(reg dest, reg a, reg b);
	triple_byte make_jz(reg a, reg ip);
	triple_byte make_load64(reg dest, reg src);
	double_byte make_load_sp(reg dest);
	triple_byte make_mv(reg dest, byte a);
	double_byte make_push(uint8_t bytes, reg src);
	double_byte make_push8(reg src);
	double_byte make_push16(reg src);
	double_byte make_push32(reg src);
	double_byte make_push64(reg src);
	double_byte make_pop(uint8_t bytes, reg src);
	double_byte make_pop8(reg dest);
	double_byte make_pop16(reg dest);
	double_byte make_pop32(reg dest);
	double_byte make_pop64(reg dest);
	triple_byte make_call(reg chunk, reg ip);
	double_byte make_ret(byte a);
	double_byte make_jmp(reg dest);

	// far_lbl is used to refer to an instruction and the chunk it is a part of

	struct far_lbl
	{
		far_lbl() : chunk_id(0), ip(0) {}
		far_lbl(uint64_t chunk, uint64_t ip) : chunk_id(chunk), ip(ip) {}
		uint64_t chunk_id; 
		uint64_t ip;
	};

	// near_lbl is used to refer to an instruction within a chunk of bytecode

	struct near_lbl 
	{
		near_lbl(uint64_t i) : ip(i) {}
		uint64_t ip; 
	};
	inline near_lbl operator+(const near_lbl& a, const near_lbl& b) { return near_lbl(a.ip + b.ip); }
	inline near_lbl operator-(const near_lbl& a, const near_lbl& b) { return near_lbl(a.ip - b.ip); }

	class bytecode
	{
		std::vector<byte> instructions;

	public:
		// Adds the bytes to the end of this bytecode, returning the address of the first byte
		near_lbl add_instruction(any_byte);
		
		// Adds the vector of bytes to the end of this bytecode, returning the address of the first byte
		near_lbl add_instructions(std::vector<any_byte>);
		
		// Returns the four bytes starting at the given address
		quad_byte get_instruction(near_lbl) const;

		// Sets the instruction at the given address to the new bytes 
		void set_instruction(near_lbl, any_byte);
		
		// Returns true if the given address maps to an instruction
		bool has_instruction(near_lbl) const;

		operator std::string() const;
	};

	class program
	{
		std::vector<bytecode> chunks;

	public:
		uint8_t add_chunk(bytecode);
		bytecode& get_chunk(uint8_t);
		size_t nr_of_chunks();
		quad_byte get_instruction(far_lbl);

		void print();
	};
}


namespace std
{
	template<> struct hash<fe::vm::reg>
	{
		size_t operator()(const fe::vm::reg& r) const
		{
			return std::hash<uint64_t>()(r.val);
		}
	};
}
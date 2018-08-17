#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	constexpr size_t stack_size = 8192;
	constexpr size_t register_count = 64;
	constexpr uint8_t
		ip_reg = register_count - 1,
		sp_reg = register_count - 2,
		fp_reg = register_count - 3,
		ret_reg = register_count - 4;

	struct machine_state
	{
		machine_state() : stack(), registers() {}
		uint8_t stack[stack_size];
		int64_t registers[64];

		void push8(uint8_t);
		void push16(uint16_t);
		void push32(uint32_t);
		void push64(uint64_t);
		uint8_t pop8();
		uint16_t pop16();
		uint32_t pop32();
		uint64_t pop64();
	};

	machine_state interpret(executable&);
}
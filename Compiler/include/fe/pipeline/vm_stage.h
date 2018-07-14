#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	constexpr size_t stack_size = 8192;
	struct machine_state
	{
		uint64_t stack_ptr = 0;
		uint64_t instruction_ptr = 0;
		uint64_t chunk_id = 0;

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

	machine_state interpret(program&);
}
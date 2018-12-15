#pragma once
#include "fe/data/bytecode.h"

namespace fe::vm
{
	constexpr size_t stack_size = 2*8192;
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
		void ret(uint8_t);
	};

	enum class vm_implementation {
		cpp,
		asm_
	};

	struct vm_settings
	{
		vm_settings()
			: print_code(false), print_result(false), print_time(false), implementation(vm_implementation::asm_), should_optimize(true) {}
		vm_settings(vm_implementation imp, bool print_code, bool print_result, bool print_time, bool so)
			: print_code(print_code), print_result(print_result), print_time(print_time), implementation(imp), should_optimize(so) {}

		bool print_code, print_result, print_time, should_optimize;
		vm_implementation implementation;
	};

	machine_state interpret(executable&, vm_settings& = vm_settings{});
}
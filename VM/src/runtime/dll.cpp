#include "dll.h"
#include <Windows.h>
#include <cstdlib>
#include <stdint.h>
#include <iostream>

namespace fe::vm
{
	struct dll
	{
		HINSTANCE windows_dll;
	};

	struct fn
	{
		typedef void (__stdcall *func_ptr)();
		func_ptr ptr;
	};

	static_assert(sizeof(dll*) == 8);
	static_assert(sizeof(fn*) == 8);
	void load_dll(uint8_t * regs, uint8_t in_count, uint8_t out_reg)
	{
		HINSTANCE dll_windows = LoadLibrary("./StdLib.dll");
		if (!dll_windows)
		{
			std::cerr << "Cannot load StdLib\n";
			std::exit(-1);
		}

		*reinterpret_cast<uint64_t*>(regs + out_reg) = reinterpret_cast<uint64_t>(new dll{ dll_windows });
	}

	void load_fn(uint8_t * regs, uint8_t in_count, uint8_t out_reg)
	{
		dll* input = reinterpret_cast<dll*>(regs + in_count);
		fn::func_ptr f = (fn::func_ptr)GetProcAddress(input->windows_dll, "TestFn");
		if (!f)
		{
			std::cerr << "Cannot load TestFn";
			std::exit(-1);
		}

		*reinterpret_cast<uint64_t*>(regs + out_reg) = reinterpret_cast<uint64_t>(new fn{ f });
	}
}
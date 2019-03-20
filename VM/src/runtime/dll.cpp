#include "dll.h"
#include <Windows.h>
#include <cstdlib>
#include <stdint.h>

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
	dll* load_dll(uint64_t a)
	{
		HINSTANCE dll_windows = LoadLibrary("./StdLib.dll");
		if (!dll_windows)
		{
			std::exit(-1);
		}
		return new dll{ dll_windows };
	}

	fn* load_fn(dll* dll_, const char* name)
	{
		fn::func_ptr f = (fn::func_ptr)GetProcAddress(dll_->windows_dll, name);
		if (!f)
		{
			std::exit(-1);
		}

		return new fn{ f };
	}
}
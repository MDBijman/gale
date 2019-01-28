#include "ffi.h"
#include <Windows.h>
#include <cstdlib>

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

	dll* load_dll(const char* location)
	{
		HINSTANCE dll_windows = LoadLibrary(location);
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
#pragma once

namespace fe::vm
{
	struct dll;
	struct fn;

	dll* load_dll(const char* location);
	fn* load_fn(dll*, const char* name);
}
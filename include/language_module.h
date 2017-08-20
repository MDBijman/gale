#pragma once
#include "std_types.h"
#include "reader.h"

namespace fe
{
	namespace language_module
	{
		environment load(const pipeline& pipeline)
		{
			environment e{};
			e.add_module("std", fe::stdlib::types::load());

			auto language_module_contents = tools::files::read_file("./snippets/language_module.fe");
			std::tie(std::ignore, std::ignore, e) = pipeline.run_to_interp(std::move(language_module_contents), std::move(e));
			return e;
		}
	}
}
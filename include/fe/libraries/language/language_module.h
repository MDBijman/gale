#pragma once
#include "fe/data/type_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/pipeline/pipeline.h"
#include "utils/reading/reader.h"
#include "fe/libraries/std/std_types.h"

namespace fe
{
	namespace language_module
	{
		std::tuple<type_environment, runtime_environment, scope_environment> load(pipeline& pipeline)
		{
			type_environment te{};
			runtime_environment re{};
			scope_environment se{};

			auto[types_te, types_re, types_se] = fe::stdlib::typedefs::load();

			te.add_module("std", std::move(types_te));
			re.add_module("std", std::move(types_re));
			se.add_module("std", std::move(types_se));

			auto language_module_contents = utils::files::read_file("./snippets/language_module.fe");
			auto [res, te, re, se] = pipeline.process(std::move(std::get<std::string>(language_module_contents)), 
				std::move(te), std::move(re), std::move(se));

			return { te, re, se };
		}
	}
}
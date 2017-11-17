#pragma once
#include "fe/data/typecheck_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/pipeline/pipeline.h"
#include "utils/reading/reader.h"
#include "fe/libraries/std/std_types.h"

namespace fe
{
	namespace language_module
	{
		std::tuple<typecheck_environment, runtime_environment> load(const pipeline& pipeline)
		{
			typecheck_environment te{};
			runtime_environment re{};

			te.add_module(fe::stdlib::types::load());

			auto language_module_contents = utils::files::read_file("./snippets/language_module.fe");
			auto result_or_error = pipeline.process(std::move(std::get<std::string>(language_module_contents)), std::move(te), std::move(re));
			std::tie(std::ignore, te, re) = std::get<std::tuple<values::unique_value, typecheck_environment, runtime_environment>>(result_or_error);

			te.name = "language";
			re.name = "language";

			return { te, re };
		}
	}
}
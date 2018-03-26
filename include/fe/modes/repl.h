#pragma once
#include "fe/language_definition.h"
#include "fe/libraries/std/std_input.h"
#include "fe/libraries/std/std_ui.h"
#include "fe/libraries/core/core_operations.h"
#include "utils/reading/reader.h"


namespace fe
{
	class repl
	{
	public:
		explicit repl(fe::pipeline pipeline) : pipeline(std::move(pipeline)) {}

		void run()
		{
			// Load modules
			auto runtime_environment = fe::runtime_environment{};
			auto type_environment = fe::type_environment{};
			auto name_environment = resolution::scope_environment{};

			// Core (global namespace)
			{
				auto[re, te, se] = fe::core::operations::load();
				runtime_environment.add_global_module(std::move(re));
				type_environment.add_global_module(std::move(te));
				name_environment.add_global_module(std::move(se));
			}

			// Std lib
			{
				auto std_runtime = fe::runtime_environment{};
				auto std_typeset = fe::type_environment{};
				auto std_nameset = resolution::scope_environment{};

				// IO utilities
				{
					auto[te, re, se] = fe::stdlib::input::load();
					std_nameset.add_module("io", std::move(se));
					std_typeset.add_module("io", std::move(te));
					std_runtime.add_module("io", std::move(re));
				}

				// UI utilities
				{
					auto[te, re, se] = fe::stdlib::ui::load();
					std_nameset.add_module("ui", std::move(se));
					std_typeset.add_module("ui", std::move(te));
					std_runtime.add_module("ui", std::move(re));
				}

				// Standard types
				{
					auto[te, re, se] = fe::stdlib::ui::load();
					std_nameset.add_global_module(std::move(se));
					std_typeset.add_global_module(std::move(te));
					std_runtime.add_global_module(std::move(re));
				}

				type_environment.add_module("std", std::move(std_typeset));
				runtime_environment.add_module("std", std::move(std_runtime));
				name_environment.add_module("std", std::move(std_nameset));
			}

			while (1) {
				std::cout << ">>> ";
				std::string code;
				std::getline(std::cin, code);

				if (code == "")
					continue;

				if (code == "env")
				{
					std::cout << type_environment.to_string(true) << std::endl;
					std::cout << runtime_environment.to_string(true) << std::endl;
					continue;
				}

				if (code == "exit")
				{
					exit(0);
				}

				if (code.size() > 4 && code.substr(0, 4) == "load")
				{
					auto filename = code.substr(5, code.size() - 4);
					auto file_or_error = utils::files::read_file(filename);
					if (std::holds_alternative<std::exception>(file_or_error))
					{
						std::cout << "File not found\n";
						continue;
					}

					code = std::get<std::string>(file_or_error);
				}

				auto lexed = pipeline.lex(std::move(code));
				auto parsed = pipeline.parse(std::move(lexed));
				auto ne = pipeline.resolve(*parsed, resolution::scope_environment(name_environment));
				auto typechecked = pipeline.typecheck(std::move(parsed), fe::type_environment(type_environment));
				auto lowered = pipeline.lower(std::move(typechecked.first));
				auto interped = pipeline.interp(std::move(lowered), fe::runtime_environment(runtime_environment));

				std::cout << std::get<fe::values::unique_value>(interped)->to_string() << std::endl;

				auto te = typechecked.second;
				auto re = interped.second;
				type_environment.add_global_module(std::move(te));
				runtime_environment.add_global_module(std::move(re));
				name_environment.add_global_module(std::move(ne));
			}
		}


	private:
		fe::pipeline pipeline;
	};
}
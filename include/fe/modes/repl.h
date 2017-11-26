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
			auto typecheck_environment = fe::typecheck_environment{};

			// Core (global namespace)
			{
				auto core = fe::core::operations::load();
				runtime_environment.add_module(std::move(std::get<fe::runtime_environment>(core)));
				typecheck_environment.add_module(std::move(std::get<fe::typecheck_environment>(core)));
			}

			// Std lib
			{
				auto std_runtime = fe::runtime_environment{};
				std_runtime.name = "std";

				auto std_typeset = fe::typecheck_environment{};
				std_typeset.name = "std";

				// IO utilities
				{
					auto[te, re] = fe::stdlib::input::load();
					std_typeset.add_module(std::move(te));
					std_runtime.add_module(std::move(re));
				}

				// UI utilities
				{
					auto[te, re] = fe::stdlib::ui::load();
					te.name = "ui";
					std_typeset.add_module(std::move(te));
					re.name = "ui";
					std_runtime.add_module(std::move(re));
				}

				// Standard types
				{
					auto[te, re] = fe::stdlib::ui::load();
					std_typeset.add_module(std::move(te));
					std_runtime.add_module(std::move(re));;
				}

				typecheck_environment.add_module(std::move(std_typeset));
				runtime_environment.add_module(std::move(std_runtime));
			}

			while (1) {
				std::cout << ">>> ";
				std::string code;
				std::getline(std::cin, code);

				if (code == "")
					continue;

				if (code == "env")
				{
					std::cout << typecheck_environment.to_string(true) << std::endl;
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
				pipeline.resolve(*parsed);
				auto typechecked = pipeline.typecheck(std::move(parsed), fe::typecheck_environment(typecheck_environment));
				auto lowered = pipeline.lower(std::move(typechecked.first));
				auto interped = pipeline.interp(std::move(lowered), fe::runtime_environment(runtime_environment));

				std::cout << std::get<fe::values::unique_value>(interped)->to_string() << std::endl;

				auto te = typechecked.second;
				auto re = interped.second;
				re.name = te.name;
				typecheck_environment.add_module(std::move(te));
				runtime_environment.add_module(std::move(re));
			}
		}


	private:
		fe::pipeline pipeline;
	};
}
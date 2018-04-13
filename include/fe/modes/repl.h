#pragma once
#include "fe/language_definition.h"
#include "fe/libraries/std/std_input.h"
#include "fe/libraries/std/std_ui.h"
#include "fe/libraries/core/core_operations.h"
#include "utils/reading/reader.h"
#include "project.h"


namespace fe
{
	class repl
	{
		project proj;

	public:
		explicit repl(fe::pipeline pipeline) : proj(std::move(pipeline)) {}

		void run()
		{
			// core
			{
				auto core_scope = fe::core::operations::load();
				proj.add_module({ "_core" }, core_scope);
			}

			// std io
			{
				auto io_scope = fe::stdlib::input::load();
				proj.add_module({ "std", "io" }, io_scope);
			}

			// std ui
			{
				auto ui_scope = fe::stdlib::ui::load();
				proj.add_module({ "std", "ui" }, ui_scope);
			}

			// std types
			{
				auto type_scope = fe::stdlib::typedefs::load();
				proj.add_module({ "std" }, type_scope);
			}

			while (1) {
				std::cout << ">>> ";
				std::string code;
				std::getline(std::cin, code);

				if (code == "")
					continue;

				if (code == "env")
				{
					//std::cout << te.to_string(true) << std::endl;
					//std::cout << re.to_string(true) << std::endl;
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

				proj.eval(code);
			}
		}
	};
}
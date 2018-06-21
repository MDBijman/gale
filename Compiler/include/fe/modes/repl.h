#pragma once
#include "fe/language_definition.h"
#include "fe/libraries/std/std_io.h"
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
			proj.add_module(fe::core::operations::load());
			// std io
			proj.add_module(fe::stdlib::io::load());
			// std ui
			proj.add_module(fe::stdlib::ui::load());
			// std types
			proj.add_module(fe::stdlib::typedefs::load());

			while (1) {
				try {
					std::cout << ">>> ";
					std::string code;
					std::getline(std::cin, code);

					if (code == "")
						continue;

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
				catch (const lexing::error& e)
				{
					std::cout << "Lexing error:\n" << e.message << "\n";
				}
				catch (const fe::parse_error& e)
				{
					std::cout << "Parse error:\n" << e.message << "\n";
				}
				catch (const fe::typecheck_error& e)
				{
					std::cout << "Typechecking error:\n" << e.message << "\n";
				}
				catch (const fe::lower_error& e)
				{
					std::cout << "Lowering error:\n" << e.message << "\n";
				}
				catch (const fe::interp_error& e)
				{
					std::cout << "Interp error:\n" << e.message << "\n";
				}
				catch (const fe::resolution_error& e)
				{
					std::cout << "Resolution error:\n" << e.message << "\n";
				}
				catch (const fe::type_env_error& e)
				{
					std::cout << e.message << "\n" << std::endl;
				}
				catch (const fe::other_error& e)
				{
					std::cout << e.message << "\n" << std::endl;
				}
				catch (const std::runtime_error& e)
				{
					std::cout << e.what() << std::endl;
				}
			}
		}
	};
}

#include "fe/modes/build.h"

#include "fe/runtime/io.h"
#include "fe/runtime/types.h"

namespace fe
{
	/*
	 * Implementation of build_settings.
	 */

	build_settings::build_settings()
	    : print_code(false), print_result(false), print_time(false), should_optimize(true)
	{
	}

	build_settings::build_settings(bool print_code, bool print_result, bool print_time, bool so)
	    : print_code(print_code), print_result(print_result), print_time(print_time),
	      should_optimize(so)
	{
	}

	build_settings &build_settings::set_input_files(std::vector<std::string> files)
	{
		this->input_files = std::move(files);
		return *this;
	}

	build_settings &build_settings::set_output_file(std::string file)
	{
		this->output_file = std::move(file);
		return *this;
	}

	build_settings &build_settings::set_available_modules(std::vector<std::string> modules)
	{
		this->modules = std::move(modules);
		return *this;
	}

	build_settings &build_settings::set_main_module(const std::string &module)
	{
		this->main_module = module;
		return *this;
	}

	bool build_settings::has_available_module(const std::string &name) const
	{
		return std::find(modules.begin(), modules.end(), name) != modules.end();
	}

	/*
	 * Implementation of builder.
	 */

	builder::builder(build_settings settings) : settings(settings), pl(fe::pipeline())
	{
	}

	int builder::run()
	{
		// Add configured modules
		if (settings.has_available_module("std.io")) add_module(fe::stdlib::io::load());

		if (settings.has_available_module("std")) add_module(fe::stdlib::typedefs::load());

		try
		{

			/*
			 * Load code files
			 */

			std::unordered_map<std::string, std::string> filename_to_code;
			for (auto file : settings.input_files)
			{
				filename_to_code.insert({ file, pl.read(file) });
			}

			/*
			 * Parse input files
			 */

			std::unordered_map<std::string, ext_ast::ast> filename_to_ast;
			for (auto pair : filename_to_code)
			{
				filename_to_ast.insert({ pair.first, pl.parse(pair.second) });
			}

			/*
			 * Parse interfaces from ASTs
			 */

			interfaces project_interfaces;
			for (auto pair : filename_to_ast)
			{
				project_interfaces.push_back(pl.extract_interface(pair.second));
			}

			/*
			 * Lower ASTs
			 */

			std::unordered_map<std::string, core_ast::ast> module_to_core_ast;
			for (auto pair : filename_to_ast)
			{
				pl.typecheck(pair.second, project_interfaces);
				module_to_core_ast.insert({ pair.first, pl.lower(pair.second) });
			}

			/*
			 * Generate module bytecode (and optionally optimize)
			 */

			std::unordered_map<std::string, vm::module> module_to_module;
			for (auto pair : module_to_core_ast)
			{
				auto module = pl.generate(pair.second);

				if (settings.should_optimize) pl.optimize_module(module);

				module_to_module.insert({ pair.first, module });
			}

			/*
			 * Link bytecode modules together (and optionally optimize)
			 */

			auto executable = pl.link(module_to_module);

			if (settings.should_optimize) pl.optimize_executable(executable);

			/*
			 * Print bytecode to file
			 */

			pl.print_bytecode("out/test.bc", executable);

			return 0;
		}
		catch (const fe::error &e)
		{
			std::cout << e.what() << std::endl;
			return 1;
		}
		catch (const std::runtime_error &e)
		{
			std::cout << e.what() << std::endl;
			return 1;
		}
	}

	void builder::add_module(module m)
	{
		modules.insert({ m.name, m });
	}

} // namespace fe
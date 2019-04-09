#include "fe/modes/build.h"

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
		this->modules = modules;
		return *this;
	}

	build_settings &build_settings::set_main_module(const std::string &module)
	{
		this->main_module = module;
		return *this;
	}

	bool build_settings::has_available_module(const std::string &name)
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

			std::unordered_map<std::string, std::string> filename_to_code;
			for (auto file : settings.input_files)
			{
				filename_to_code.insert({ file, pl.read(file) });
			}

			std::unordered_map<std::string, ext_ast::ast> filename_to_ast;
			for (auto pair : filename_to_code)
			{
				filename_to_ast.insert({ pair.first, pl.parse(pair.second) });
			}

			/*std::unordered_map<std::string, interface> module_to_interface;
			for (auto pair : filename_to_ast)
			{
				module_to_interface.insert(
				  { pair.first, pl.extract_interface(pair.second) });
			}*/

			std::unordered_map<std::string, core_ast::ast> module_to_core_ast;
			for (auto pair : filename_to_ast)
			{
                // #todo Pass interfaces
				pl.typecheck(pair.second);
				module_to_core_ast.insert({ pair.first, pl.lower(pair.second) });
			}

			std::unordered_map<std::string, module> module_to_module;
			for (auto pair : module_to_core_ast)
			{
				module_to_module.insert({ pair.first, pl.generate(pair.second) });
			}

			auto program = pl.link(module_to_module);

			if (settings.should_optimize)
			{
				pl.optimize_program(program);
			}

			pl.print_bytecode("out/test.bc", program);

			return 0;
		}
        catch (const fe::error& e)
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

	module builder::compile(const std::string &code)
	{
		auto e_ast = pl.parse(code);

		auto &root_node = e_ast.get_node(e_ast.root_id());
		root_node.type_scope_id = e_ast.create_type_scope();
		root_node.name_scope_id = e_ast.create_name_scope();

		// Add name and type scopes of imports
		auto imports = e_ast.get_imports();
		if (imports)
		{
			for (const ext_ast::identifier &imp : *imports)
			{
				auto pos = modules.find(imp.full_path());

				if (pos == modules.end())
				{
					throw other_error{ "Cannot find module: " +
							   imp.operator std::string() };
				}

				auto module_name_scope = e_ast.create_name_scope();
				e_ast.get_name_scope(module_name_scope).merge(pos->second.names);
				e_ast.get_name_scope(root_node.name_scope_id)
				  .add_module(imp.full_path(), module_name_scope);

				auto module_type_scope = e_ast.create_type_scope();
				e_ast.get_type_scope(module_type_scope).merge(pos->second.types);
				e_ast.get_type_scope(root_node.type_scope_id)
				  .add_module(imp.full_path(), module_type_scope);
			}
		}

		// Stage 1: typecheck
		pl.typecheck(e_ast);

		// Stage 2: lower (desugar)
		auto c_ast = pl.lower(e_ast);
		auto &core_root_node = c_ast.get_node(c_ast.root_id());

		// Stage 3: generate
		auto bytecode = pl.generate(c_ast);

		if (imports)
		{
			for (const ext_ast::identifier &imp : *imports)
			{
				auto pos = modules.find(imp.full_path());
				if (pos == modules.end())
					throw other_error{ "Cannot find module: " +
							   imp.operator std::string() };

				for (auto c : pos->second.code)
				{
					auto full_name = imp.full + "." + c.get_name();
					bytecode.add_function(
					  c.is_bytecode()
					    ? vm::function(full_name, c.get_bytecode())
					    : vm::function(full_name, c.get_native_function_id()));
				}
			}
		}

		if (settings.should_optimize)
		{
			pl.optimize_program(bytecode);
		}

		if (settings.print_code)
		{
			std::cout << bytecode.to_string();
		}

		auto executable = pl.link(bytecode);

		// optimize
		pl.optimize_executable(executable);

		return executable;
	}

	void builder::add_module(module m)
	{
		modules.insert({ m.name, m });
	}

} // namespace fe
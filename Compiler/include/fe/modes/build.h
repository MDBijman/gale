#pragma once
#include "fe/data/ext_ast.h"
#include "fe/data/module.h"
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "fe/pipeline/pretty_print_stage.h"
#include "fe/runtime/io.h"
#include "fe/runtime/types.h"
#include "utils/reading/reader.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fe
{
	struct build_settings
	{
		build_settings()
		    : print_code(false), print_result(false), print_time(false),
		      should_optimize(true)
		{
		}
		build_settings(bool print_code, bool print_result, bool print_time, bool so)
		    : print_code(print_code), print_result(print_result), print_time(print_time),
		      should_optimize(so)
		{
		}

		build_settings &set_input_files(std::vector<std::string> files)
		{
			this->input_files = std::move(files);
			return *this;
		}

		build_settings &set_output_file(std::string file)
		{
			this->output_file = std::move(file);
			return *this;
		}

		build_settings &set_available_modules(std::vector<std::string> modules)
		{
			this->modules = modules;
			return *this;
		}

		bool has_available_module(const std::string &name)
		{
			return std::find(modules.begin(), modules.end(), name) != modules.end();
		}

		std::vector<std::string> input_files;
		std::string output_file;
		std::vector<std::string> modules;

		bool print_code, print_result, print_time, should_optimize;
	};

	class builder
	{
	      public:
		builder(build_settings settings) : settings(settings), pl(fe::pipeline()) {}

		int run()
		{
			// Add configured modules
			if (settings.has_available_module("std.io"))
				add_module(fe::stdlib::io::load());
			if (settings.has_available_module("std"))
				add_module(fe::stdlib::typedefs::load());

			// Check input file sizes #todo implement multi file compilation
			if (settings.input_files.size() > 1)
			{
				std::cerr
				  << "Compilation of more than a single file not yet supported\n";
				return 1;
			}

			// Log input file
			auto input_file = settings.input_files[0];
			auto file_path = std::filesystem::path(input_file);
			std::cout << "Compiling: " << file_path.filename() << "\n";

			// Load input file
			auto file_or_error = utils::files::read_file(file_path.string());
			if (std::holds_alternative<std::exception>(file_or_error))
			{
				std::cerr << "File " << file_path.filename() << " not found\n";
				return 1;
			}
			auto &code = std::get<std::string>(file_or_error);

			try
			{
				compile_to_file(settings.output_file, code);
				return 0;
			}
			catch (const lexing::error &e)
			{
				std::cout << "Lexing error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::parse_error &e)
			{
				std::cout << "Parse error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::typecheck_error &e)
			{
				std::cout << "Typechecking error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::lower_error &e)
			{
				std::cout << "Lowering error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::interp_error &e)
			{
				std::cout << "Interp error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::resolution_error &e)
			{
				std::cout << "Resolution error:\n" << e.message << "\n";
				return 1;
			}
			catch (const fe::type_env_error &e)
			{
				std::cout << e.message << "\n" << std::endl;
				return 1;
			}
			catch (const fe::other_error &e)
			{
				std::cout << e.message << "\n" << std::endl;
				return 1;
			}
			catch (const std::runtime_error &e)
			{
				std::cout << e.what() << std::endl;
				return 1;
			}
		}

		vm::executable compile(const std::string &code)
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
						throw other_error{ "Cannot find module: " +
								   imp.operator std::string() };

					auto module_name_scope = e_ast.create_name_scope();
					e_ast.get_name_scope(module_name_scope)
					  .merge(pos->second.names);
					e_ast.get_name_scope(root_node.name_scope_id)
					  .add_module(imp.full_path(), module_name_scope);

					auto module_type_scope = e_ast.create_type_scope();
					e_ast.get_type_scope(module_type_scope)
					  .merge(pos->second.types);
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
						    : vm::function(full_name,
								   c.get_native_function_id()));
					}
				}
			}

			if (settings.should_optimize)
			{
				// optimize
				pl.optimize_program(bytecode);
			}

			if (settings.print_code)
			{
				auto &e = bytecode;
				auto &funs = e.get_code();
				for (int i = 0; i < funs.size(); i++)
				{
					auto &fun = funs[i];
					if (!fun.is_bytecode()) continue;
					auto &bc = fun.get_bytecode();

					std::cout << "\n" << fun.get_name() << "\n";
					std::string out;
					size_t ip = 0;
					while (bc.has_instruction(ip))
					{
						auto in = bc.get_instruction<10>(ip);
						if (vm::byte_to_op(in[0].val) == vm::op_kind::NOP)
						{
							ip++;
							continue;
						}

						out += std::to_string(ip) + ": ";
						out +=
						  vm::op_to_string(vm::byte_to_op(in[0].val)) + " ";
						for (int i = 1;
						     i < vm::op_size(vm::byte_to_op(in[0].val));
						     i++)
							out += std::to_string(in[i].val) + " ";

						out += "\n";
						ip += vm::op_size(vm::byte_to_op(in[0].val));
					}

					std::cout << out;
				}
			}
			auto executable = pl.link(bytecode);

			// optimize
			pl.optimize_executable(executable);

			return executable;
		}

		void compile_to_file(const std::string &filename, const std::string &code)
		{
			auto e = compile(code);
			pl.print_bytecode(filename, e);
		}

		void add_module(module m) { modules.insert({ m.name, m }); }

	      private:
		std::unordered_map<module_name, module> modules;
		build_settings settings;
		fe::pipeline pl;
	};
} // namespace fe
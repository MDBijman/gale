#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "fe/data/ext_ast.h"
#include "fe/data/module.h"
#include "fe/runtime/types.h"
#include "utils/reading/reader.h"
#include "fe/pipeline/pretty_print_stage.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace fe
{
	struct project_settings
	{
		project_settings()
			: print_code(false), print_result(false), print_time(false), should_optimize(true) {}
		project_settings(bool print_code, bool print_result, bool print_time, bool so)
			: print_code(print_code), print_result(print_result), print_time(print_time), should_optimize(so) {}

		bool print_code, print_result, print_time, should_optimize;
	};

	class project
	{
	public:
		project(fe::pipeline pipeline) : pl(std::move(pipeline)) {}

		void add_module(module m)
		{
			modules.insert({ m.name, m });
		}

		vm::executable compile(const std::string& code, project_settings s)
		{
			auto e_ast = pl.parse(code);

			auto& root_node = e_ast.get_node(e_ast.root_id());
			root_node.type_scope_id = e_ast.create_type_scope();
			root_node.name_scope_id = e_ast.create_name_scope();

			// Add name and type scopes of imports
			auto imports = e_ast.get_imports();
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.full_path());
					if (pos == modules.end())
						throw other_error{ "Cannot find module: " + imp.operator std::string() };

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
			auto& core_root_node = c_ast.get_node(c_ast.root_id());

			// Stage 3: generate
			auto bytecode = pl.generate(c_ast);

			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.full_path());
					if (pos == modules.end())
						throw other_error{ "Cannot find module: " + imp.operator std::string() };

					for (auto c : pos->second.code)
					{
						auto full_name = imp.full + "." + c.get_name();
						bytecode.add_function(c.is_bytecode() ?
							vm::function(full_name, c.get_bytecode()) :
							vm::function(full_name, c.get_native_function_id()));
					}
				}
			}

			if (s.should_optimize)
			{
				// optimize
				pl.optimize_program(bytecode);
			}
	
			if(s.print_code)
			{
				auto& e = bytecode;
				auto& funs = e.get_code();
				for (int i = 0; i < funs.size(); i++)
				{
					auto& fun = funs[i];
					if (!fun.is_bytecode()) continue;
					auto& bc = fun.get_bytecode();

					std::cout << "\n" << fun.get_name() << "\n";
					std::string out;
					size_t ip = 0;
					while (bc.has_instruction(ip))
					{
						auto in = bc.get_instruction<10>(ip);
						if (vm::byte_to_op(in[0].val) == vm::op_kind::NOP)
						{
							ip++; continue;
						}

						out += std::to_string(ip) + ": ";
						out += vm::op_to_string(vm::byte_to_op(in[0].val)) + " ";
						for (int i = 1; i < vm::op_size(vm::byte_to_op(in[0].val)); i++)
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

		void compile_to_file(const std::string& filename, const std::string& code, project_settings s)
		{
			auto e = compile(code, s);
			pl.print_bytecode(filename, e);
		}

	private:
		std::unordered_map<module_name, module> modules;
		fe::pipeline pl;
	};
}
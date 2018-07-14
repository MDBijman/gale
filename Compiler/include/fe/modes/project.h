#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "fe/data/ext_ast.h"
#include "fe/data/module.h"
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_ui.h"
#include "fe/libraries/std/std_io.h"
#include "fe/libraries/std/std_types.h"
#include "utils/reading/reader.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace fe
{
	class project
	{
	public:
		project(fe::pipeline pipeline) : pl(std::move(pipeline)) {}

		void add_module(module m)
		{
			modules.insert({ m.name, m });
		}

		vm::machine_state eval(const std::string& code)
		{
			auto e_ast = pl.parse(code);

			auto& root_node = e_ast.get_node(e_ast.root_id());
			root_node.type_scope_id = e_ast.create_type_scope();
			root_node.name_scope_id = e_ast.create_name_scope();

			module& core_module = modules.at(module_name{ "_core" });
			{
				// Name scope
				auto core_name_scope = e_ast.create_name_scope();
				e_ast.get_name_scope(core_name_scope).merge(core_module.names);
				e_ast.get_name_scope(root_node.name_scope_id)
					.add_module({ "_core" }, core_name_scope);

				// Type scope
				auto core_type_scope = e_ast.create_type_scope();
				e_ast.get_type_scope(core_type_scope).merge(core_module.types);
				e_ast.get_type_scope(root_node.type_scope_id)
					.add_module({ "_core" }, core_type_scope);
			}

			// Add name and type scopes of imports
			auto imports = e_ast.get_imports();
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.copy_name());
					if (pos == modules.end())
						throw other_error{ "Cannot find module: " + imp.operator std::string() };

					auto module_name_scope = e_ast.create_name_scope();
					e_ast.get_name_scope(module_name_scope).merge(pos->second.names);
					e_ast.get_name_scope(root_node.name_scope_id)
						.add_module(imp.segments, module_name_scope);

					auto module_type_scope = e_ast.create_type_scope();
					e_ast.get_type_scope(module_type_scope).merge(pos->second.types);
					e_ast.get_type_scope(root_node.type_scope_id)
						.add_module(imp.segments, module_type_scope);
				}
			}

			// Stage 1: typecheck
			pl.typecheck(e_ast);

			// Stage 2: lower (desugar)
			auto c_ast = pl.lower(e_ast);
			auto& core_root_node = c_ast.get_node(c_ast.root_id());

			// Value scope
			{
				auto core_value_scope = c_ast.create_value_scope();
				c_ast.get_runtime_context().get_scope(core_value_scope).merge(core_module.values);
				c_ast.get_runtime_context().add_module({ "_core" }, core_value_scope);
			}

			// Add value scopes of imports
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.copy_name());
					assert(pos != modules.end());

					auto module_value_scope = c_ast.create_value_scope();
					c_ast.get_runtime_context().get_scope(module_value_scope).merge(pos->second.values);
					c_ast.get_runtime_context().add_module(imp.segments, module_value_scope);
				}
			}

			// Stage 3: interpret
			auto bytecode = pl.generate(c_ast);
			bytecode.print();
			return pl.run(bytecode);
		}

	private:
		std::unordered_map<module_name, module> modules;
		fe::pipeline pl;
	};
}
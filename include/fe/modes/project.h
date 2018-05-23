#pragma once
#include "fe/pipeline/pipeline.h"
#include "fe/language_definition.h"
#include "utils/reading/reader.h"
#include "fe/data/ext_ast.h"
#include "fe/data/scope.h"
#include "fe/libraries/core/core_operations.h"
#include "fe/libraries/std/std_ui.h"
#include "fe/libraries/std/std_output.h"
#include "fe/libraries/std/std_types.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace fe
{
	using module_name = std::vector<std::string>;
}

namespace std
{
	template<> struct hash<fe::module_name>
	{
		size_t operator()(const fe::module_name& o) const
		{
			size_t h = 0;
			for (const auto& name : o)
				h ^= hash<string>()(name);
			return h;
		}
	};
}

namespace fe
{
	class project
	{
	public:
		project(fe::pipeline pipeline) : pl(std::move(pipeline)) {}

		void add_module(const module_name& id, scope m)
		{
			modules.insert({ id, m });
		}

		void add_module(std::string code)
		{
			auto tokens = pl.lex(std::move(code));
			auto ast = pl.parse(std::move(tokens));

			auto module_scope = eval(ast);
			auto& name = ast.get_module_name().value().segments;

			add_module(name, module_scope);
		}

		scope eval(std::string code)
		{
			auto tokens = pl.lex(std::move(code));
			auto ast = pl.parse(std::move(tokens));
			return eval(ast);
		}

		scope eval(ext_ast::ast& ast)
		{
			auto& root_node = ast.get_node(ast.root_id());
			scope& core_module = modules.at(module_name{ "_core" });
			{
				// Name scope
				auto core_name_scope = ast.create_name_scope();
				ast.get_name_scope(core_name_scope).merge(core_module.name_env());
				ast.get_name_scope(*root_node.name_scope_id)
					.add_module(ext_ast::identifier("_core"), core_name_scope);

				// Type scope
				auto core_type_scope = ast.create_type_scope();
				ast.get_type_scope(core_type_scope).merge(core_module.type_env());
				ast.get_type_scope(*root_node.type_scope_id)
					.add_module(ext_ast::identifier("_core"), core_type_scope);
			}

			// Add name and type scopes of imports
			auto imports = ast.get_imports();
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.segments);
					if (pos == modules.end())
						throw other_error{ "Cannot find module: " + imp.operator std::string() };

					auto module_name_scope = ast.create_name_scope();
					ast.get_name_scope(module_name_scope).merge(pos->second.name_env());
					ast.get_name_scope(*root_node.name_scope_id)
						.add_module(imp, module_name_scope);

					auto module_type_scope = ast.create_type_scope();
					ast.get_type_scope(module_type_scope).merge(pos->second.type_env());
					ast.get_type_scope(*root_node.type_scope_id)
						.add_module(imp, module_type_scope);
				}
			}

			// Stage 1: typecheck
			pl.typecheck(ast);

			// Stage 2: lower (desugar)
			auto core_ast = pl.lower(ast);
			auto& core_root_node = core_ast.get_node(core_ast.root_id());

			// Value scope
			{
				auto core_value_scope = core_ast.create_value_scope();
				core_ast.get_value_scope(core_value_scope).merge(core_module.value_env());
				core_ast.get_value_scope(*core_root_node.value_scope_id)
					.add_module(core_ast::identifier("_core"), core_value_scope);
			}

			// Add value scopes of imports
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.segments);
					assert(pos != modules.end());

					auto module_value_scope = core_ast.create_value_scope();
					core_ast.get_value_scope(module_value_scope).merge(pos->second.value_env());
					core_ast.get_value_scope(*core_root_node.value_scope_id)
						.add_module(
							core_ast::identifier(imp.without_last_segment().segments, imp.segments.back(), 0, {}),
							module_value_scope);
				}
			}

			// Stage 3: interpret
			pl.interp(core_ast);

			return scope(
				core_ast.get_value_scope(*core_root_node.value_scope_id),
				ast.get_type_scope(*root_node.type_scope_id),
				ast.get_name_scope(*root_node.name_scope_id)
			);
		}

	private:
		std::unordered_map<module_name, scope> modules;
		fe::pipeline pl;
	};
}
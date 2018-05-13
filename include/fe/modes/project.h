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

			// Name scope
			ext_ast::name_scope& root_name_scope = ast.get_name_scope(*root_node.name_scope_id);
			root_name_scope.add_module(ext_ast::identifier("_core"), &core_module.name_env());

			// Type scope
			ext_ast::type_scope& root_type_scope = ast.get_type_scope(*root_node.type_scope_id);
			root_type_scope.add_module(ext_ast::identifier("_core"), &core_module.type_env());

			// Add name and type scopes of imports
			auto imports = ast.get_imports();
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.segments);
					assert(pos != modules.end());

					root_name_scope.add_module(imp, &pos->second.name_env());
					root_type_scope.add_module(imp, &pos->second.type_env());
				}
			}

			// Stage 1: typecheck
			pl.typecheck(ast);

			// Stage 2: lower (desugar)
			auto core_ast = pl.lower(ast);
			auto& core_root_node = core_ast.get_node(core_ast.root_id());

			// Value scope
			value_scope& root_value_scope = core_ast.get_value_scope(*core_root_node.value_scope_id);
			root_value_scope.add_module(core_ast::identifier("_core"), &core_module.value_env());

			// Add value scopes of imports
			if (imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.segments);
					assert(pos != modules.end());
					root_value_scope.add_module(
						core_ast::identifier(imp.without_last_segment().segments, imp.segments.back(), 0, {}),
						&pos->second.value_env());
				}
			}

			// Stage 3: interpret
			pl.interp(core_ast);

			return scope(
				root_value_scope,
				root_type_scope,
				root_name_scope
			);
		}

	private:
		std::unordered_map<module_name, scope> modules;
		fe::pipeline pl;
	};
}
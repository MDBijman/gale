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
			ext_ast::name_scope& root_name_scope = ast.get_name_scope(*root_node.name_scope_id);
			ext_ast::type_scope& root_type_scope = ast.get_type_scope(*root_node.type_scope_id);
			runtime_environment runtime_env;

			if (auto imports = ast.get_imports(); imports)
			{
				for (const ext_ast::identifier& imp : *imports)
				{
					auto pos = modules.find(imp.segments);
					assert(pos != modules.end());

					root_name_scope.add_module(imp, &pos->second.name_env());
					root_type_scope.add_module(imp, &pos->second.type_env());
					runtime_env.add_module(imp.segments, pos->second.runtime_env());
				}
			}

			scope& core_module = modules.at(module_name{ "_core" });
			root_name_scope.add_module(ext_ast::identifier{ {"_core"} }, &core_module.name_env());
			root_type_scope.add_module(ext_ast::identifier{ {"_core"} }, &core_module.type_env());
			runtime_env.add_module("_core", core_module.runtime_env());

			pl.typecheck(ast);
			auto core_ast = pl.lower(ast);
			auto re = pl.interp(*core_ast, runtime_env);

			return scope(
				re,
				ast.get_type_scope(root_node.type_scope_id.value()),
				ast.get_name_scope(root_node.name_scope_id.value())
			);
		}

	private:
		std::unordered_map<module_name, scope> modules;
		fe::pipeline pl;
	};
}
#pragma once
#include "fe/data/extended_ast.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/type_environment.h"
#include "fe/pipeline/pipeline.h"

#include <string>
#include <vector>
#include <memory>

namespace fe
{
	using module_name = std::vector<std::string>;

	class module
	{
	public:
		virtual ~module() = 0 {};

		virtual const module_name& get_name() = 0;
		virtual std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p) = 0;
	};

	class code_module : public module
	{
	public:
		code_module(module_name name, extended_ast::unique_node&& root) : name(name), root(std::move(root)) {}
	
		const module_name& get_name() override 
		{ 
			return name; 
		}

		std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p)
		{
			type_environment te;
			runtime_environment re;
			scope_environment se;

			for (auto& import : imports)
			{
				auto[sub_te, sub_re, sub_se] = import->interp(p);
				if (import->get_name().size() == 1 && import->get_name()[0] == "core")
				{
					te.add_global_module(std::move(sub_te));
					se.add_global_module(std::move(sub_se));
					re.add_global_module(std::move(sub_re));
				}
				else
				{
					te.add_module(import->get_name(), std::move(sub_te));
					se.add_module(import->get_name(), std::move(sub_se));
					re.add_module(import->get_name(), std::move(sub_re));
				}
			}

			auto new_se = p.resolve(*root, std::move(se));
			auto[new_root, new_te] = p.typecheck(std::move(root), std::move(te));
			auto core_root = p.lower(std::move(new_root));
			auto[new_core_root, new_re] = p.interp(std::move(core_root), std::move(re));

			return{ std::move(new_te), std::move(new_re), std::move(new_se) };
		}

		extended_ast::unique_node root;

		module_name name;
		std::vector<std::shared_ptr<module>> imports;
	};

	class native_module : public module
	{
	public:
		native_module(module_name name, runtime_environment re, type_environment te, scope_environment se)
			: name(name), runtime_environment(std::move(re)), type_environment(std::move(te)),
			scope_environment(std::move(se)) {}

		native_module(std::string name, runtime_environment re, type_environment te, scope_environment se)
			: native_module(std::vector<std::string>{name}, std::move(re),std::move(te), std::move(se)) {}

		const module_name& get_name() override
		{
			return name;
		}

		std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p)
		{
			return { type_environment, runtime_environment, scope_environment };
		}

		module_name name;
		scope_environment scope_environment;
		runtime_environment runtime_environment;
		type_environment type_environment;
	};
}
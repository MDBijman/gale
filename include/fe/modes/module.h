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
	class module
	{
	public:
		virtual ~module() = 0 {};

		virtual const std::string& get_name() = 0;
		virtual std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p) = 0;
	};

	class code_module : public module
	{
	public:
		code_module(std::string module_name, extended_ast::unique_node&& root) : name(module_name), root(std::move(root)) {}
	
		const std::string& get_name() override { return name; }
		std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p)
		{
			type_environment te;
			runtime_environment re;
			scope_environment se;
			re.name = name;

			for (auto& import : imports)
			{
				auto[sub_te, sub_re, sub_se] = import->interp(p);
				if (import->get_name() == "core")
				{
					te.add_global_module(std::move(sub_te));
					se.add_global_module(std::move(sub_se));
				}
				else
				{
					te.add_module(import->get_name(), std::move(sub_te));
					se.add_module(import->get_name(), std::move(sub_se));
				}

				re.add_module(std::move(sub_re));
			}

			auto new_se = p.resolve(*root, std::move(se));
			auto[new_root, new_te] = p.typecheck(std::move(root), std::move(te));
			auto core_root = p.lower(std::move(new_root));
			auto[new_core_root, new_re] = p.interp(std::move(core_root), std::move(re));

			return{ std::move(new_te), std::move(new_re), std::move(new_se) };
		}

		extended_ast::unique_node root;

		std::string name;
		std::vector<std::shared_ptr<module>> imports;
	};

	class native_module : public module
	{
	public:
		native_module(std::string module_name, runtime_environment re, type_environment te, scope_environment se)
			: name(module_name), native_runtime_environment(std::move(re)), native_type_environment(std::move(te)),
			native_scope_environment(std::move(se)) {}

		const std::string& get_name() override { return name; }
		std::tuple<type_environment, runtime_environment, scope_environment> interp(pipeline& p)
		{
			return { native_type_environment, native_runtime_environment, native_scope_environment };
		}

		std::string name;
		scope_environment native_scope_environment;
		runtime_environment native_runtime_environment;
		type_environment native_type_environment;
	};
}
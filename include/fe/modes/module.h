#pragma once
#include "fe/data/extended_ast.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/typecheck_environment.h"

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
		virtual std::pair<typecheck_environment, runtime_environment> interp(pipeline& p) = 0;
	};

	class code_module : public module
	{
	public:
		code_module(std::string module_name, extended_ast::unique_node&& root) : name(module_name), root(std::move(root)) {}
	
		const std::string& get_name() override { return name; }
		std::pair<typecheck_environment, runtime_environment> interp(pipeline& p)
		{
			typecheck_environment te;
			te.name = name;
			runtime_environment re;
			re.name = name;

			for (auto& import : imports)
			{
				auto[sub_te, sub_re] = import->interp(p);
				te.add_module(std::move(sub_te));
				re.add_module(std::move(sub_re));
			}

			p.resolve(*root);
			auto[new_root, new_te] = p.typecheck(std::move(root), std::move(te));
			auto core_root = p.lower(std::move(new_root));
			auto[new_core_root, new_re] = p.interp(std::move(core_root), std::move(re));

			return{ std::move(new_te), std::move(new_re) };
		}

		extended_ast::unique_node root;

		std::string name;
		std::vector<std::shared_ptr<module>> imports;
	};

	class native_module : public module
	{
	public:
		native_module(std::string module_name, runtime_environment re, typecheck_environment te) : name(module_name), native_runtime_environment(std::move(re)), native_type_environment(std::move(te)) {}

		const std::string& get_name() override { return name; }
		std::pair<typecheck_environment, runtime_environment> interp(pipeline& p)
		{
			return { native_type_environment, native_runtime_environment };
		}

		std::string name;
		runtime_environment native_runtime_environment;
		typecheck_environment native_type_environment;
	};
}
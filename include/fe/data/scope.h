#pragma once
#include "fe/data/runtime_environment.h"
#include "fe/data/type_scope.h"
#include "fe/data/name_scope.h"

namespace fe
{
	class scope
	{
		runtime_environment re;
		ext_ast::type_scope te;
		ext_ast::name_scope se;

	public:
		scope(runtime_environment re, ext_ast::type_scope ts, ext_ast::name_scope ns) : 
			re(std::move(re)), te(std::move(ts)), se(std::move(ns)) {}

		runtime_environment& runtime_env()
		{
			return re;
		}

		ext_ast::type_scope& type_env()
		{
			return te;
		}

		ext_ast::name_scope& name_env()
		{
			return se;
		}

		void merge(scope o)
		{
			this->re.add_global_module(std::move(o.runtime_env()));
			this->te.merge(std::move(o.type_env()));
			this->se.merge(std::move(o.name_env()));
		}
	};
}
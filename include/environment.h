#pragma once
#include "types.h"
#include "values.h"

namespace fe
{
	struct type_environment
	{
		type_environment() {}
		type_environment(std::unordered_map<std::string, types::type> type_mapping) : types(type_mapping) {}
		type_environment(const type_environment& other) : types(other.types) {}

		void set(const std::string& name, types::type type)
		{
			types.insert_or_assign(name, type);
		}

		types::type get(const std::string& name)
		{
			return types.at(name);
		}

	private:
		std::unordered_map<std::string, types::type> types;
	};

	struct value_environment
	{
		value_environment() {}
		value_environment(std::unordered_map<std::string, std::shared_ptr<values::value>> values) : values(values) {}
		value_environment(const value_environment& other) : values(other.values) {}

		void set(const std::string& name, std::shared_ptr<values::value> value)
		{
			values.insert_or_assign(name, value);
		}

		std::shared_ptr<values::value> get(const std::string& name)
		{
			return values.at(name);
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<values::value>> values;
	};

	class environment
	{
	public:
		void add_module(std::string name, std::shared_ptr<values::value> module)
		{
			modules.insert({ name, module });
		}

		type_environment t_env;
		value_environment v_env;
	private:
		std::unordered_multimap<std::string, std::shared_ptr<values::value>> modules;
	};
}
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

		types::type get(const std::string& name) const
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

		std::shared_ptr<values::value> get(const std::string& name) const
		{
			return values.at(name);
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<values::value>> values;
	};

	class environment
	{
	public:
		void add_module(std::string name, environment module)
		{
			modules.insert({ name, module });
		}

		types::type typeof(const std::vector<std::string>& identifier) const
		{
			if (identifier.size() == 1)
				return type_environment.get(identifier.at(0));
			else return modules.find(identifier.at(0))->second.typeof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
		}

		types::type typeof(const std::string& name) const
		{
			return type_environment.get(name);
		}

		std::shared_ptr<values::value> valueof(const std::string& name) const
		{
			return value_environment.get(name);
		}

		std::shared_ptr<values::value> valueof(const std::vector<std::string>& identifier) const
		{
			if (identifier.size() == 1)
				return value_environment.get(identifier.at(0));
			else return modules.find(identifier.at(0))->second.valueof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
		}

		void set_value(const std::string& identifier, std::shared_ptr<values::value> value)
		{
			value_environment.set(identifier, value);
		}

		void set_value(const std::vector<std::string>& identifier, std::shared_ptr<values::value> value)
		{
			if (identifier.size() == 1)
				value_environment.set(identifier.at(0), value);
			else 
				modules.find(identifier.at(0))->second.set_value(std::vector<std::string>{ identifier.begin() + 1, identifier.end() }, value);
		}

		void set_type(const std::string& identifier, types::type type)
		{
			type_environment.set(identifier, type);
		}

		void set_type(const std::vector<std::string>& identifier, types::type type)
		{
			if (identifier.size() == 1)
				type_environment.set(identifier.at(0), type);
			else 
				modules.find(identifier.at(0))->second.set_type(std::vector<std::string>{ identifier.begin() + 1, identifier.end() }, type);
		}

	private:
		type_environment type_environment;
		value_environment value_environment;

		std::unordered_multimap<std::string, environment> modules;
	};
}
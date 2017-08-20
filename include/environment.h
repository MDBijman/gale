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

		void extend(type_environment&& other)
		{
			types.insert(std::make_move_iterator(other.types.begin()), std::make_move_iterator(other.types.end()));
		}

		void set(const std::string& name, types::type type)
		{
			types.insert_or_assign(name, type);
		}

		types::type get(const std::string& name) const
		{
			return types.at(name);
		}

		std::string to_string()
		{
			std::string r("type_environment (");

			uint32_t counter = 0;
			for(auto& pair : types)
			{
				std::string t = pair.first + ": ";
				t.append(std::visit(types::to_string, pair.second));
				r.append(t);

				if (counter != types.size() - 1)
				{
					r.append(", ");
				}

				counter++;
			}

			r.append(")");

			return r;
		}

	private:
		std::unordered_map<std::string, types::type> types;
	};

	struct value_environment
	{
		value_environment() {}
		value_environment(std::unordered_map<std::string, values::value> values) : values(values) {}
		value_environment(const value_environment& other) : values(other.values) {}

		void extend(value_environment&& other)
		{
			values.insert(std::make_move_iterator(other.values.begin()), std::make_move_iterator(other.values.end()));
		}

		void set(const std::string& name, values::value&& value)
		{
			values.insert_or_assign(name, value);
		}

		const values::value& get(const std::string& name) const
		{
			return values.at(name);
		}

		std::string to_string()
		{
			std::string r("value_environment (");

			uint32_t counter = 0;
			for(auto& pair : values)
			{
				std::string t = pair.first + ": ";
				t.append(std::visit(values::to_string, pair.second));
				r.append(t);

				if (counter != values.size() - 1)
				{
					r.append(", ");
				}

				counter++;
			}

			r.append(")");
			return r;
		}

	private:
		std::unordered_map<std::string, values::value> values;
	};

	class environment
	{
	public:
		void add_module(std::string name, environment module)
		{
			modules.insert({ name, module });
		}

		void extend(environment&& other)
		{
			type_environment.extend(std::move(other.type_environment));
			value_environment.extend(std::move(other.value_environment));
			modules.insert(std::make_move_iterator(other.modules.begin()), std::make_move_iterator(other.modules.end()));
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

		const values::value& valueof(const std::string& name) const
		{
			return value_environment.get(name);
		}

		const values::value& valueof(const std::vector<std::string>& identifier) const
		{
			if (identifier.size() == 1)
				return value_environment.get(identifier.at(0));
			else return modules.find(identifier.at(0))->second.valueof(std::vector<std::string>{ identifier.begin() + 1, identifier.end() });
		}

		void set_value(const std::string& identifier, values::value&& value)
		{
			value_environment.set(identifier, std::move(value));
		}

		void set_value(const std::vector<std::string>& identifier, values::value&& value)
		{
			if (identifier.size() == 1)
				value_environment.set(identifier.at(0), std::move(value));
			else 
				modules.find(identifier.at(0))->second.set_value(std::vector<std::string>{ identifier.begin() + 1, identifier.end() }, std::move(value));
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

		std::string to_string()
		{
			std::string r("environment (");
			r.append(type_environment.to_string());
			r.append(", ");
			r.append(value_environment.to_string());
			r.append(")");
			return r;
		}

	private:
		type_environment type_environment;
		value_environment value_environment;

		std::unordered_multimap<std::string, environment> modules;
	};
}
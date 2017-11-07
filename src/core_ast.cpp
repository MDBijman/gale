#include "core_ast.h"
#include "values.h"
#include "runtime_environment.h"
#include <vector>

namespace fe
{
	namespace core_ast
	{
		// No op

		no_op::no_op() : type(types::unset_type()) {}

		no_op::no_op(const no_op& other) : type(other.type) {}
		no_op& no_op::operator=(const no_op& other)
		{
			type = other.type;
			return *this;
		}
		no_op::no_op(no_op&& other) : type(std::move(other.type)) {}
		no_op& no_op::operator=(no_op&& other)
		{
			this->type = std::move(other.type);
			return *this;
		}

		values::value no_op::interp(runtime_environment &)
		{
			return values::void_value();
		}

		// Integer

		integer::integer(values::integer val) : type(types::atom_type("i32")), value(val) {}
		
		integer::integer(const integer& other) : value(other.value), type(other.type) {}
		integer& integer::operator=(const integer& other)
		{
			this->value = other.value;
			this->type = other.type;
			return *this;
		}
		integer::integer(integer&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		integer& integer::operator=(integer&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		values::value integer::interp(runtime_environment &)
		{
			return this->value;
		}

		// String

		string::string(values::string val) : type(types::atom_type("str")), value(val) {}
		
		string::string(const string& other) : value(other.value), type(other.type) {}
		string& string::operator=(const string& other)
		{
			this->value = other.value;
			this->type = other.type;
			return *this;
		}
		string::string(string&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		string& string::operator=(string&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		values::value string::interp(runtime_environment &)
		{
			return this->value;
		}

		// Identifier

		identifier::identifier(std::vector<std::string>&& modules, std::string&& name, std::vector<int>&& offsets) : modules(std::move(modules)), variable_name(std::move(name)), offsets(std::move(offsets)) {};

		identifier::identifier(const identifier& other) : modules(other.modules), variable_name(other.variable_name), offsets(other.offsets), type(other.type) {}
		identifier& identifier::operator=(const identifier& other)
		{
			this->modules = other.modules;
			this->variable_name = other.variable_name;
			this->offsets = other.offsets;
			this->type = other.type;
			return *this;
		}
		identifier::identifier(identifier&& other) : modules(std::move(other.modules)), variable_name(std::move(other.variable_name)), offsets(std::move(other.offsets)), type(std::move(other.type)) {}
		identifier& identifier::operator=(identifier&& other)
		{
			this->modules = std::move(other.modules);
			this->variable_name = std::move(other.variable_name);
			this->offsets = std::move(other.offsets);
			this->type = std::move(other.type);
			return *this;
		}

		values::value identifier::interp(runtime_environment& env)
		{
			return env.valueof(*this);
		}

		// Set

		set::set(identifier&& id, unique_node&& value, types::type t) : id(std::move(id)), value(std::move(value)), type(t) {};

		set::set(const set& other) : id(other.id), value(unique_node(other.value->copy())), type(other.type) {};
		set& set::operator=(const set& other)
		{
			this->id = other.id;
			this->type = other.type;
			this->value = unique_node(other.value->copy());
			return *this;
		}
		set::set(set&& other) : id(std::move(other.id)), value(std::move(other.value)), type(std::move(other.type)) {};
		set& set::operator=(set&& other)
		{
			this->id = std::move(other.id);
			this->type = std::move(other.type);
			this->value = std::move(other.value);
			return *this;
		}

		values::value set::interp(runtime_environment& env)
		{
			auto val = this->value->interp(env);
			env.set_value(this->id.variable_name, values::value(val));
			return val;
		}

		// Function

		function::function(std::optional<identifier>&& name, std::variant<std::vector<identifier>, identifier>&& parameters, unique_node&& body, types::type t) : name(std::move(name)), parameters(std::move(parameters)), type(t), body(std::move(body)) {}
		
		function::function(const function& other) : name(other.name), parameters(other.parameters), body(other.body->copy()), type(other.type) {}
		function& function::operator=(const function& other)
		{
			this->name = other.name;
			this->parameters = other.parameters;
			this->body.reset(other.body->copy());
			this->type = other.type;
			return *this;
		}
		function::function(function&& other) : name(std::move(other.name)), parameters(std::move(other.parameters)), body(std::move(other.body)), type(std::move(other.type)) {}
		function& function::operator=(function&& other)
		{
			this->name = std::move(other.name);
			this->parameters = std::move(other.parameters);
			this->body = std::move(other.body);
			this->type = std::move(other.type);
			return *this;
		}

		values::value function::interp(runtime_environment& env)
		{
			return values::function(unique_node(this->copy()));
		}

		// Tuple

		tuple::tuple(std::vector<unique_node> children, types::type t) : type(t), children(std::move(children)) {}
		
		tuple::tuple(const tuple& other) : type(other.type) 
		{
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
		}
		tuple& tuple::operator=(const tuple& other)
		{
			this->children.clear();
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
			this->type = other.type;
			return *this;
		}
		tuple::tuple(tuple&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
		tuple& tuple::operator=(tuple&& other)
		{
			this->type = std::move(other.type);
			this->children = std::move(other.children);
			return *this;
		}

		values::value tuple::interp(runtime_environment& env)
		{
			std::vector<values::value> res;
			for (decltype(auto) tuple_node : this->children)
			{
				res.push_back(tuple_node->interp(env));
			}

			return values::tuple(std::move(res));
		}

		// Block

		block::block(std::vector<unique_node> children, types::type t) : type(t), children(std::move(children)) {}

		block::block(const block& other) : type(other.type) 
		{
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
		}
		block& block::operator=(const block& other)
		{
			this->children.clear();
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
			this->type = other.type;
			return *this;
		}
		block::block(block&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
		block& block::operator=(block&& other)
		{
			this->type = std::move(other.type);
			this->children = std::move(other.children);
			return *this;
		}

		values::value block::interp(runtime_environment& env)
		{
			values::value res;
			for (decltype(auto) block_node : this->children)
			{
				res = block_node->interp(env);
			}
			return res;
		}

		// Function Call

		function_call::function_call(identifier&& id, unique_node&& parameter, types::type&& t)
			: type(std::move(t)), id(std::move(id)), parameter(std::move(parameter)) {}

		function_call::function_call(const function_call& other) : id(other.id), parameter(other.parameter->copy()), type(other.type) {}
		function_call& function_call::operator=(const function_call& other)
		{
			this->id = other.id;
			this->parameter.reset(other.parameter->copy());
			this->type = other.type;
			return *this;
		}
		function_call::function_call(function_call&& other) : id(std::move(other.id)), parameter(std::move(other.parameter)), type(std::move(other.type)) {}
		function_call& function_call::operator=(function_call&& other)
		{
			this->id = std::move(other.id);
			this->parameter = std::move(other.parameter);
			this->type = std::move(other.type);
			return *this;
		}

		values::value function_call::interp(runtime_environment& env)
		{
			auto& function_value = env.valueof(this->id);

			runtime_environment function_env(env);

			auto param_value = parameter->interp(function_env);

			// Function written in cpp
			if (std::holds_alternative<values::native_function>(function_value))
			{
				auto& function = std::get<values::native_function>(function_value);

				// Eval function
				return function.function(move(param_value));
			}
			// Function written in language itself
			else if (std::holds_alternative<values::function>(function_value))
			{
				auto function = dynamic_cast<core_ast::function*>(std::get<values::function>(function_value).func.get());

				// TODO improve
				runtime_environment function_env;
				if (id.modules.size() > 0)
					function_env = env.get_module(id.modules.at(0));
				else
					function_env = env;

				if (std::holds_alternative<std::vector<core_ast::identifier>>(function->parameters))
				{
					auto& param_tuple = std::get<values::tuple>(param_value);

					auto& parameters = std::get<std::vector<core_ast::identifier>>(function->parameters);
					for (auto i = 0; i < parameters.size(); i++)
					{
						auto& id = parameters.at(i);
						function_env.set_value(id.variable_name, std::move(param_tuple.content.at(i)));
					}
				}
				else
				{
					// Single argument function
					auto& parameter = std::get<core_ast::identifier>(function->parameters);
					function_env.set_value(parameter.variable_name, std::move(param_value));
				}

				return function->body->interp(function_env);
			}
		}

		// Branch

		branch::branch(unique_node test, unique_node true_path, unique_node false_path) : test_path(std::move(test)), true_path(std::move(true_path)), false_path(std::move(false_path)) {}

		branch::branch(const branch& other) :
			test_path(unique_node(other.test_path->copy())), true_path(unique_node(other.true_path->copy())), false_path(unique_node(other.false_path->copy())), type(other.type) {}
		branch& branch::operator=(const branch& other)
		{
			this->test_path = unique_node(other.test_path->copy());
			this->true_path = unique_node(other.true_path->copy());
			this->false_path = unique_node(other.false_path->copy());
			this->type = other.type;
			return *this;
		}
		branch::branch(branch&& other) : test_path(std::move(other.test_path)), true_path(std::move(other.true_path)), false_path(std::move(other.false_path)), type(std::move(other.type)) {}
		branch& branch::operator=(branch&& other)
		{
			this->test_path = std::move(other.test_path);
			this->true_path = std::move(other.true_path);
			this->false_path = std::move(other.false_path);
			this->type = other.type;
			return *this;
		}

		values::value branch::interp(runtime_environment& env)
		{
			auto test_res = std::get<values::boolean>(test_path->interp(env));

			if (test_res.val)
				return true_path->interp(env);
			else
				return false_path->interp(env);
		}

		reference::reference(unique_node exp) : exp(std::move(exp)) {}

		reference::reference(const reference& other) : exp(unique_node(other.exp->copy())), type(other.type) {}
		reference& reference::operator=(const reference& other)
		{
			this->exp = unique_node(other.exp->copy());
			this->type = other.type;
			return *this;
		}

		reference::reference(reference&& other) : exp(std::move(other.exp)), type(std::move(other.type)) {}
		reference& reference::operator=(reference&& other)
		{
			this->exp = std::move(other.exp);
			this->type = std::move(other.type);
			return *this;
		}

		values::value reference::interp(runtime_environment& env)
		{
			return exp->interp(env);
		}
	}
}
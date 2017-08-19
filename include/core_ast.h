#pragma once
#include "types.h"
#include "values.h"
#include <vector>

namespace fe
{
	namespace core_ast
	{
		// Value nodes

		struct integer
		{
			integer(values::integer val);
			
			// Copy
			integer(const integer& other);

			// Move
			integer(integer&& other);
			integer& operator=(integer&& other);

			values::integer value;
			types::type type;
		};

		struct string
		{
			string(values::string val);
			
			// Copy
			string(const string& other);

			// Move
			string(string&& other);
			string& operator=(string&& other);

			values::string value;
			types::type type;
		};

		struct function
		{
			function(values::function&& fun, types::type t);
			
			// Copy
			function(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			values::function fun;
			types::type type;
		};

		// Derivatives

		struct identifier;
		struct assignment;
		struct function_call;

		struct tuple
		{
			tuple(std::vector<std::variant<tuple, identifier, assignment, function_call, integer, string, function>> children, types::type t);
			
			// Copy
			tuple(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			std::vector<std::variant<tuple, identifier, assignment, function_call, integer, string, function>> children;
			types::type type;
		};

		struct identifier
		{
			identifier(std::vector<std::string>&& name, types::type t);
			identifier(std::string&& name, types::type t);

			// Copy
			identifier(const identifier& other);
			identifier& operator=(const identifier& other);

			// Move
			identifier(identifier&& other);
			identifier& operator=(identifier&& other);
			

			std::vector<std::string> name;
			types::type type;
		};

		struct assignment
		{
			assignment(identifier&& id, std::unique_ptr<std::variant<tuple, identifier, assignment, function_call, integer, string, function>>&& val);
			
			// Copy
			assignment(const assignment& other);

			// Move
			assignment(assignment&& other);
			assignment& operator=(assignment&& other);

			identifier id;
			std::unique_ptr<std::variant<tuple, identifier, assignment, function_call, integer, string, function>> value;
			types::type type;
		};

		struct function_call
		{
			function_call(identifier&& id, tuple&& params, types::type&& t);
			
			// Copy
			function_call(const function_call& other);

			// Move
			function_call(function_call&& other);
			function_call& operator=(function_call&& other);

			identifier id;
			tuple params;
			types::type type;
		};

		using node = std::variant<tuple, identifier, assignment, function_call, integer, string, function>;
		using unique_node = std::unique_ptr<node>;

		const auto make_unique = [](auto& x) {
			return std::make_unique<node>(x);
		};
	}
}
#pragma once
#include "types.h"
#include "values.h"
#include <vector>
#include <optional>

// Also defined in core_ast.cpp
#define AST_NODE std::variant<tuple, identifier, assignment, function_call, integer, string, function, conditional_branch, conditional_branch_path>

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
			integer& operator=(const integer& other);

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
			string& operator=(const string& other);

			// Move
			string(string&& other);
			string& operator=(string&& other);

			values::string value;
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

		struct assignment;
		struct function_call;
		struct conditional_branch;
		struct atom_variable;
		struct composite_variable;
		struct function_variable;

		struct function
		{
			function(std::optional<identifier>&& name, std::vector<identifier>&& parameters, std::unique_ptr<AST_NODE>&& body, types::type t);
			
			// Copy
			function(const function& other);
			function& operator=(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			// Name is set when the function is not anonymous, for recursion
			std::optional<identifier> name;
			std::vector<identifier> parameters;
			std::unique_ptr<AST_NODE> body;
			types::type type;
		};

		// Derivatives


		struct tuple
		{
			tuple(std::vector<AST_NODE> children, types::type t);
			
			// Copy
			tuple(const tuple& other);
			tuple& operator=(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			std::vector<AST_NODE> children;
			types::type type;
		};

		struct assignment
		{
			assignment(identifier&& id, std::unique_ptr<AST_NODE>&& val);
			
			// Copy
			assignment(const assignment& other);
			assignment& operator=(const assignment& other);

			// Move
			assignment(assignment&& other);
			assignment& operator=(assignment&& other);

			identifier id;
			std::unique_ptr<AST_NODE> value;
			types::type type;
		};

		struct function_call
		{
			function_call(identifier&& id, std::unique_ptr<AST_NODE>&& parameter, types::type&& t);
			
			// Copy
			function_call(const function_call& other);
			function_call& operator=(const function_call& other);

			// Move
			function_call(function_call&& other);
			function_call& operator=(function_call&& other);

			identifier id;
			std::unique_ptr<AST_NODE> parameter;
			types::type type;
		};

		struct conditional_branch_path
		{
			conditional_branch_path(std::unique_ptr<AST_NODE> test,
				std::unique_ptr<AST_NODE> code);

			// Copy
			conditional_branch_path(const conditional_branch_path& other);
			conditional_branch_path& operator=(const conditional_branch_path& other);

			// Move
			conditional_branch_path(conditional_branch_path&& other);
			conditional_branch_path& operator=(conditional_branch_path&& other);

			std::unique_ptr<AST_NODE> test_path;
			std::unique_ptr<AST_NODE> code_path;
			types::type type;
		};

		struct conditional_branch
		{
			conditional_branch(std::vector<conditional_branch_path>&& branches, types::type&& t);
	
			// Copy
			conditional_branch(const conditional_branch& other);
			conditional_branch& operator=(const conditional_branch& other);

			// Move
			conditional_branch(conditional_branch&& other);
			conditional_branch& operator=(conditional_branch&& other);

			std::vector<conditional_branch_path> branches;
			types::type type;
		};

		using node = AST_NODE;
		using unique_node = std::unique_ptr<node>;

		const auto make_unique = [](auto&& x) {
			return std::make_unique<node>(x);
		};
	}
}

#undef AST_NODE
#pragma once
#include <string>
#include <iostream>
#include <functional>
#include "types.h"

// Forward declaration of core ast
namespace fe
{
	namespace core_ast
	{
		struct tuple;
		struct identifier;
		struct assignment;
		struct function_call;
		struct integer;
		struct string;
		struct function;
		using node = std::variant<tuple, identifier, assignment, function_call, integer, string, function>;
		using unique_node = std::unique_ptr<node>;
	}
}

namespace fe
{
	namespace values
	{
		struct string
		{
			string(std::string s) : val(s) {}

			// Copy
			string(const string& other) : val(other.val) {}
			string& operator=(const string& other)
			{
				this->val = other.val;
				return *this;
			}

			// Move
			string(string&& other) : val(std::move(other.val)) {}
			string& operator=(string&& other)
			{
				this->val = std::move(other.val);
				return *this;
			}

			std::string val;

			void print()
			{
				std::cout << val;
			}
		};

		struct integer
		{
			integer(int n) : val(n) {}

			// Copy
			integer(const integer& other) = default;
			integer& operator=(const integer& other)
			{
				this->val = other.val;
				return *this;
			}

			// Move
			integer(integer&& other) : val(other.val) {}
			integer& operator=(integer&& other)
			{
				this->val = std::move(other.val);
				return *this;
			}

			int val;

			void print()
			{
				std::cout << val;
			}
		};

		struct void_value
		{
			void print()
			{
				std::cout << "void" << std::endl;
			}
		};

		struct type
		{
			type(types::type t) : kind(t) {}

			// Copy
			type(const type& other) = default;
			type& operator=(const type& other)
			{
				this->kind = other.kind;
				return *this;
			}

			// Move
			type(type&& other) : kind(std::move(other.kind)) {}
			type& operator=(type&& other)
			{
				this->kind = std::move(other.kind);
				return *this;
			}

			types::type kind;

			void print()
			{
				std::cout << "type";
			}
		};

		struct function
		{
			function(std::vector<core_ast::identifier> params, core_ast::unique_node body);

			// Copy
			function(const function& other);
			function& operator=(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			std::vector<core_ast::identifier> parameters;
			core_ast::unique_node body;

			void print();
		};

		struct tuple;
		struct native_function;

		struct module
		{
			// Copy
			module(const module& other) {}
			module& operator=(const module& other)
			{
				this->exports = other.exports;
				return *this;
			}

			// Move
			module(module&& other) {}
			module& operator=(module&& other)
			{
				this->exports = std::move(other.exports);
				return *this;
			}

			std::unordered_map<std::string, std::shared_ptr<std::variant<string, integer, tuple, function, native_function, module, type>>> exports;

			void print()
			{
				std::cout << "module";
			}
		};

		struct tuple
		{
			tuple();
			tuple(std::vector<std::variant<string, integer, tuple, function, native_function, module, type>> values);

			// Copy
			tuple(const tuple& other);
			tuple& operator=(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			std::vector<std::variant<string, integer, tuple, function, native_function, module, type>> content;

			void print();
		};

		struct native_function
		{
			native_function(std::function<std::variant<string, integer, tuple, function, native_function, module, type>(std::variant<string, integer, tuple, function, native_function, module, type>)> f) : function(f) {}

			// Copy
			native_function(const native_function& other);
			native_function& operator=(const native_function& other);

			// Move
			native_function(native_function&& other);
			native_function& operator=(native_function&& other);

			std::function<std::variant<string, integer, tuple, function, native_function, module, type>(std::variant<string, integer, tuple, function, native_function, module, type>)> function;

			void print();
		};

		using value = std::variant<string, integer, tuple, function, native_function, module, type>;
		using unique_value = std::unique_ptr<value>;
		using shared_value = std::shared_ptr<value>;

		const auto make_shared = [](auto value) {
			return std::make_shared<std::variant<string, integer, tuple, function, native_function, module, type>>(value);
		};

		const auto print_value = [](auto& value) {
			value.print();
		};
	}
}
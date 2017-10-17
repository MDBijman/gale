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
		struct node;
	}
}

// Also defined in values.cpp
#define VALUE_NODE std::variant<void_value, string, integer, boolean, tuple, function, native_function, module>

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

			std::string to_string()
			{
				return "\"" + val + "\"";
			}

			std::string val;
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

			std::string to_string()
			{
				return std::to_string(val);
			}
		};

		struct boolean
		{
			boolean(bool b) : val(b) {}

			// Copy
			boolean(const boolean& other) = default;
			boolean& operator=(const boolean& other)
			{
				this->val = other.val;
				return *this;
			}

			// Move
			boolean(boolean&& other) : val(other.val) {}
			boolean& operator=(boolean&& other)
			{
				this->val = std::move(other.val);
				return *this;
			}

			bool val;

			std::string to_string()
			{
				return val ? "true" : "false";
			}
		};

		struct void_value
		{
			std::string to_string()
			{
				return "void";
			}
		};

		struct function
		{
			function(std::unique_ptr<core_ast::node> func);

			// Copy
			function(const function& other);
			function& operator=(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			std::unique_ptr<core_ast::node> func;

			std::string to_string();
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

			std::unordered_map<std::string, std::shared_ptr<VALUE_NODE>> exports;

			std::string to_string()
			{
				return "module";
			}
		};

		struct tuple
		{
			tuple();
			tuple(std::vector<VALUE_NODE> values);

			// Copy
			tuple(const tuple& other);
			tuple& operator=(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			std::vector<VALUE_NODE> content;

			std::string to_string();
		};

		struct native_function
		{
			native_function(std::function<VALUE_NODE(VALUE_NODE)> f) : function(f) {}

			// Copy
			native_function(const native_function& other);
			native_function& operator=(const native_function& other);

			// Move
			native_function(native_function&& other);
			native_function& operator=(native_function&& other);

			std::function<VALUE_NODE(VALUE_NODE)> function;

			std::string to_string();
		};

		using value = VALUE_NODE;
		using unique_value = std::unique_ptr<value>;
		using shared_value = std::shared_ptr<value>;

		const auto make_shared = [](auto value) {
			return std::make_shared<VALUE_NODE>(value);
		};

		const auto to_string = [](auto& value) -> std::string {
			return value.to_string();
		};
	}
}

#undef VALUE_NODE
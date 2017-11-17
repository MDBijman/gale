#pragma once
#include "fe/data/types.h"

#include <string>
#include <iostream>
#include <functional>

// Forward declaration of core ast
namespace fe
{
	namespace core_ast
	{
		struct node;
		using unique_node = std::unique_ptr<node>;
	}
}

// Also defined in values.cpp
namespace fe
{
	namespace values
	{
		struct value
		{
			virtual ~value() = 0 {};
			virtual std::string to_string() const = 0;
			virtual value* copy() const = 0;
			virtual bool operator==(value* other) const = 0;
		};

		const auto make_shared = [](auto val) {
			return std::make_shared<value>(val);
		};

		const auto make_unique = [](auto val) {
			return std::unique_ptr<value>(val.copy());
		};

		using unique_value = std::unique_ptr<value>;
		using shared_value = std::shared_ptr<value>;

		template<class Derived, class Base>
		struct comparable
		{
		public:
			bool operator==(Base* o) const
			{
				if (typeid(Derived) != typeid(*o))
					return false;

				const Derived & a = static_cast<const Derived&>(*this);
				const Derived & b = static_cast<const Derived&>(*o);

				return a == b;
			}
		};

		template<class Derived, class Base>
		struct copyable
		{
		public:
			Base* copy(const Derived& o) const
			{
				return new Derived(o);
			}
		};

		struct string : public value, private comparable<string, value>, private copyable<string, value>
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

			std::string to_string() const override
			{
				return "\"" + val + "\"";
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const string& other) const
			{
				return val == other.val;
			}

			std::string val;
		};

		struct integer : public value, private comparable<integer, value>, private copyable<integer, value>
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

			std::string to_string() const override
			{
				return std::to_string(val);
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const integer& other) const
			{
				return val == other.val;
			}

			int val;
		};

		struct boolean : public value, private comparable<boolean, value>, private copyable<boolean, value>
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

			std::string to_string() const override
			{
				return val ? "true" : "false";
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const boolean& other) const
			{
				return val == other.val;
			}

			bool val;
		};

		struct void_value : public value, private comparable<void_value, value>, private copyable<void_value, value>
		{
			std::string to_string() const override
			{
				return "void";
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const void_value& other) const
			{
				return true;
			}
		};

		struct function : public value, private comparable<function, value>, private copyable<function, value>
		{
			function(core_ast::unique_node func);

			// Copy
			function(const function& other);
			function& operator=(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			std::string to_string() const override;
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const function& other) const
			{
				return func.get() == other.func.get();
			}

			core_ast::unique_node func;
		};

		struct module : public value, private comparable<module, value>, private copyable<module, value>
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

			std::string to_string() const override
			{
				return "module";
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const module& other) const
			{
				return exports == other.exports;
			}

			std::unordered_map<std::string, shared_value> exports;
		};

		struct tuple : public value, private comparable<tuple, value>, private copyable<tuple, value>
		{
			tuple();
			tuple(std::vector<unique_value> values);

			// Copy
			tuple(const tuple& other);
			tuple& operator=(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			std::string to_string() const override;
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const tuple& other) const
			{
				if (content.size() != other.content.size()) return false;

				for(int i = 0; i < other.content.size(); i++)
				{
					if (!(*content.at(i) == other.content.at(i).get()))
						return false;
				}
				return true;
			}

			std::vector<unique_value> content;
		};

		struct native_function : public value, private comparable<native_function, value>, private copyable<native_function, value>
		{
			native_function(std::function<unique_value(unique_value)> f) : function(f) {}

			// Copy
			native_function(const native_function& other);
			native_function& operator=(const native_function& other);

			// Move
			native_function(native_function&& other);
			native_function& operator=(native_function&& other);

			std::string to_string() const override;
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const native_function& other) const
			{
				return function.target<unique_value(unique_value)>() == other.function.target<unique_value(unique_value)>();
			}

			std::function<unique_value(unique_value)> function;
		};

		template<class T>
		struct custom_value : public value, private comparable<custom_value<T>, value>, private copyable<custom_value<T>, value>
		{
			custom_value(T t) : val(std::move(t)) {}

			// Copy
			custom_value(const custom_value& other) : val(other.val) {}
			custom_value& operator=(const custom_value& other)
			{
				val = other.val;
				return *this;
			}

			// Move
			custom_value(custom_value&& other) : val(std::move(other.val)) {}
			custom_value& operator=(custom_value&& other)
			{
				this->val = std::move(other.val);
				return *this;
			}

			std::string to_string() const override
			{
				return typeid(T).name();
			}
			bool operator==(value* other) const override { return comparable::operator==(other); }
			value* copy() const override { return copyable::copy(*this); }

			bool operator==(const custom_value& other) const
			{
				return val == other.val;
			}

			T val;
		};
	}
}
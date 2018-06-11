#pragma once
#include "fe/data/types.h"

#include <string>
#include <iostream>
#include <functional>

// Forward declaration of core ast
namespace fe
{
	using node_id = uint32_t;
}

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
			virtual types::unique_type get_type() const = 0;
		};

		const auto make_shared = [](auto val) {
			return std::make_shared<value>(val);
		};

		const auto make_unique = [](auto val) {
			return std::unique_ptr<value>(val.copy());
		};

		using unique_value = std::unique_ptr<value>;
		using shared_value = std::shared_ptr<value>;


		namespace detail
		{
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
				Base * copy(const Derived& o) const
				{
					return new Derived(o);
				}
			};

			template<types::atom_type Type> struct literal_value;
			template<> struct literal_value<types::atom_type::I32> { using value_type = int32_t; };
			template<> struct literal_value<types::atom_type::I64> { using value_type = int64_t; };
			template<> struct literal_value<types::atom_type::UI32> { using value_type = uint32_t; };
			template<> struct literal_value<types::atom_type::UI64> { using value_type = uint64_t; };
			template<> struct literal_value<types::atom_type::F32> { using value_type = float; };
			template<> struct literal_value<types::atom_type::F64> { using value_type = double; };
			template<> struct literal_value<types::atom_type::STRING> { using value_type = std::string; };
			template<> struct literal_value<types::atom_type::BOOL> { using value_type = bool; };

			template<types::atom_type Type>
			struct literal : public value, private comparable<literal<Type>, value>, private copyable<literal<Type>, value>
			{
				using value_type = typename literal_value<Type>::value_type;

				literal(value_type s) : val(s) {}

				// Copy
				literal(const literal& other) : val(other.val) {}
				literal& operator=(const literal& other)
				{
					this->val = other.val;
					return *this;
				}

				// Move
				literal(literal&& other) : val(std::move(other.val)) {}
				literal& operator=(literal&& other)
				{
					this->val = std::move(other.val);
					return *this;
				}

				bool operator==(value* other) const override { return comparable::operator==(other); }

				value_type operator*(const literal& other) const { return val * other.val; }
				value_type operator/(const literal& other) const { return val / other.val; }
				value_type operator%(const literal& other) const { return val % other.val; }
				value_type operator+(const literal& other) const { return val + other.val; }
				value_type operator-(const literal& other) const { return val - other.val; }
				bool operator==(const literal& other) const { return val == other.val; }
				bool operator!=(const literal& other) const { return val != other.val; }
				bool operator> (const literal& other) const { return val >  other.val; }
				bool operator>=(const literal& other) const { return val >= other.val; }
				bool operator< (const literal& other) const { return val <  other.val; }
				bool operator<=(const literal& other) const { return val <= other.val; }
				bool operator&&(const literal& other) const { return val && other.val; }
				bool operator||(const literal& other) const { return val || other.val; }
				bool operator!() const { return !val; }

				std::string to_string() const override
				{
					if constexpr (Type != types::atom_type::STRING)
					{
						return "\"" + std::to_string(val) + "\"";
					}
					else
					{
						return "\"" + val + "\"";
					}
				}

				value* copy() const override { return copyable::copy(*this); }
				types::unique_type get_type() const override { return types::unique_type(new types::atom<Type>()); }

				value_type val;
			};
		}

		using i32     = detail::literal<types::atom_type::I32>;
		using i64     = detail::literal<types::atom_type::I64>;
		using ui32    = detail::literal<types::atom_type::UI32>;
		using ui64    = detail::literal<types::atom_type::UI64>;
		using f32     = detail::literal<types::atom_type::F32>;
		using f64     = detail::literal<types::atom_type::F64>;
		using boolean = detail::literal<types::atom_type::BOOL>;
		using str     = detail::literal<types::atom_type::STRING>;

		struct void_value : public value, private detail::comparable<void_value, value>, 
			private detail::copyable<void_value, value>
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

			types::unique_type get_type() const override { return types::make_unique(types::atom<types::atom_type::VOID>()); }
		};

		struct function : public value, private detail::comparable<function, value>, 
			private detail::copyable<function, value>
		{
			function(node_id id);

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
				return func == other.func;
			}

			// #todo use typechecking result of func
			types::unique_type get_type() const override { return types::make_unique(types::atom<types::atom_type::ANY>()); }

			node_id func;
		};

		struct tuple : public value, private detail::comparable<tuple, value>, 
			private detail::copyable<tuple, value>
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
				if (val.size() != other.val.size()) return false;

				for (int i = 0; i < other.val.size(); i++)
				{
					if (!(*val.at(i) == other.val.at(i).get()))
						return false;
				}
				return true;
			}

			types::unique_type get_type() const override
			{
				types::sum_type st;
				for (auto& val : val)
				{
					st.sum.push_back(val->get_type());
				}
				return types::make_unique(st);
			}

			std::vector<unique_value> val;
		};

		struct native_function : public value, private detail::comparable<native_function, value>, 
			private detail::copyable<native_function, value>
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
				// Just compare pointers, maybe a better solution is possible
				return this == &other;
			}

			types::unique_type get_type() const override
			{
				return types::make_unique(types::voidt());
			}

			std::function<unique_value(unique_value)> function;
		};

		template<class T>
		struct custom_value : public value, private detail::comparable<custom_value<T>, value>, 
			private detail::copyable<custom_value<T>, value>
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

			types::unique_type get_type() const override
			{
				return types::make_unique(types::voidt());
			}

			T val;
		};
	}
}
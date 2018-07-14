#pragma once
#include <vector>
#include <variant>
#include <memory>
#include <optional>
#include <assert.h>

namespace fe
{
	namespace types
	{
		namespace detail
		{
			template<class Derived, class Base>
			struct comparable
			{
			public:
				bool operator==(Base* o) const
				{
					if (typeid(*o) == typeid(types::any))
						return true;

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
		}

		struct type
		{
			virtual ~type() = 0 {};
			virtual operator std::string() const = 0;
			virtual type* copy() const = 0;
			virtual size_t calculate_size() const = 0;
			virtual bool operator==(type* other) const = 0;
		};

		using unique_type = std::unique_ptr<type>;

		// Atom types

		enum class atom_type
		{
			I32, 
			I64,
			UI32, 
			UI64,
			F32, 
			F64, 
			BOOL, 
			STRING,
			UNSET, 
			ANY, 
			VOID
		};

		constexpr const char* atom_type_str(atom_type lit)
		{
			switch (lit)
			{
			case atom_type::I32:       return "std.i32";    break;
			case atom_type::I64:       return "std.i64";    break;
			case atom_type::UI32:      return "std.ui32";   break;
			case atom_type::UI64:      return "std.ui64";   break;
			case atom_type::F32:       return "std.f32";    break;
			case atom_type::F64:       return "std.f64";    break;
			case atom_type::STRING:    return "std.str";    break;
			case atom_type::BOOL:      return "std.bool";   break;
			case atom_type::UNSET:     return "unset";      break;
			case atom_type::ANY:       return "any";        break;
			case atom_type::VOID:      return "void";       break;
			}
			assert(!"Unknown atom type");
			throw std::runtime_error("Unknown atom type");
		};

		constexpr size_t atom_type_size(atom_type lit)
		{
			switch (lit)
			{
			case atom_type::I32:       return 4;  break;
			case atom_type::I64:       return 8;  break;
			case atom_type::UI32:      return 4;  break;
			case atom_type::UI64:      return 8;  break;
			case atom_type::F32:       return 4;  break;
			case atom_type::F64:       return 8;  break;
			case atom_type::STRING:    return 8;  break;
			case atom_type::BOOL:      return 1;  break;
			case atom_type::UNSET:     return -1; break;
			case atom_type::ANY:       return -1; break;
			case atom_type::VOID:      return 0;  break;
			}
			assert(!"Unknown atom type");
			throw std::runtime_error("Unknown atom type");
		}

		template<atom_type Type>
		struct atom : public type, private detail::comparable<atom<Type>, type>, private detail::copyable<atom<Type>, type>
		{
			atom() {}

			// Move
			atom(atom&& other) {}
			atom& operator=(atom&& other) {}

			// Copy
			atom(const atom& other) {}
			atom& operator=(const atom& other) {}

			operator std::string() const override
			{
				return atom_type_str(Type);
			}

			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override { return atom_type_size(Type); }
		};

		using i32        = atom<atom_type::I32>;
		using i64        = atom<atom_type::I64>;
		using ui32       = atom<atom_type::UI32>;
		using ui64       = atom<atom_type::UI64>;
		using f32        = atom<atom_type::F32>;
		using f64        = atom<atom_type::F64>;
		using boolean    = atom<atom_type::BOOL>;
		using str        = atom<atom_type::STRING>;
		using any        = atom<atom_type::ANY>;
		using unset      = atom<atom_type::UNSET>;
		using voidt      = atom<atom_type::VOID>;

		// Composition types

		struct sum_type : public type, private detail::comparable<sum_type, type>, private detail::copyable<sum_type, type>
		{
			sum_type();
			sum_type(std::vector<unique_type> sum);

			// Move
			sum_type(sum_type&& other);
			sum_type& operator=(sum_type&& other);

			// Copy
			sum_type(const sum_type& other);
			sum_type& operator=(const sum_type& other);

			operator std::string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{ 
				size_t max = 0;
				for (auto& t : sum)
				{
					size_t s = t->calculate_size();
					max = s > max ? s : max;
				}
				return max;
			}

			std::vector<unique_type> sum;
		};

		struct array_type : public type, private detail::comparable<array_type, type>, private detail::copyable<array_type, type>
		{
			array_type();
			array_type(unique_type t);
			array_type(type& t);

			// Move
			array_type(array_type&& other);
			array_type& operator=(array_type&& other);

			// Copy
			array_type(const array_type& other);
			array_type& operator=(const array_type& other);

			operator std::string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				return 4;
			}

			unique_type element_type;
		};

		struct reference_type : public type, private detail::comparable<reference_type, type>, private detail::copyable<reference_type, type>
		{
			reference_type();
			reference_type(unique_type t);
			reference_type(type& t);

			// Move
			reference_type(reference_type&& other);
			reference_type& operator=(reference_type&& other);

			// Copy
			reference_type(const reference_type& other);
			reference_type& operator=(const reference_type& other);

			operator std::string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				return 4;
			}

			unique_type referred_type;
		};

		struct product_type : public type, private detail::comparable<product_type, type>, private detail::copyable<product_type, type>
		{
			product_type();
			product_type(std::vector<unique_type> product);

			// Move
			product_type(product_type&& other);
			product_type& operator=(product_type&& other);

			// Copy
			product_type(const product_type& other);
			product_type& operator=(const product_type& other);

			operator std::string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				size_t sum = 0;
				for (auto& t : product) sum += t->calculate_size();
				return sum;
			}

			std::vector<unique_type> product;
		};

		struct function_type : public type, private detail::comparable<function_type, type>, private detail::copyable<function_type, type>
		{
			function_type(unique_type f, unique_type t);
			function_type(const type& f, const type& t);

			// Move
			function_type(function_type&& other);
			function_type& operator=(function_type&& other);

			// Copy
			function_type(const function_type& other);
			function_type& operator=(const function_type& other);

			operator std::string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				return to->calculate_size();
			}

			unique_type from, to;
		};


		// Helper methods

		const auto make_unique = [](auto& x) {
			return std::unique_ptr<type>(x.copy());
		};

		// Operators

		template<atom_type T>
		bool operator==(const atom<T>& one, const atom<T>& two)
		{
			return true;
		}

		bool operator==(const sum_type& one, const sum_type& two);

		bool operator==(const product_type& one, const product_type& two);

		bool operator==(const function_type& one, const function_type& two);

		bool operator==(const array_type& one, const array_type& two);

		bool operator==(const reference_type& one, const reference_type& two);
	}
}
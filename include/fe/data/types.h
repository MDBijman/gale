#pragma once
#include <vector>
#include <variant>
#include <memory>
#include <optional>

namespace fe
{
	namespace types
	{
		struct type
		{
			virtual ~type() = 0 {};
			virtual std::string to_string() const = 0;
			virtual type* copy() const = 0;
			virtual bool operator==(type* other) const = 0;
		};

		using unique_type = std::unique_ptr<type>;

		template<class Derived, class Base>
		struct comparable 
		{
		public:
			bool operator==(Base* o) const
			{
				if (typeid(*o) == typeid(any_type))
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
			Base* copy(const Derived& o) const
			{
				return new Derived(o);
			}
		};

		// Atomic types
		struct atom_type : public type, private comparable<atom_type, type>, private copyable<atom_type, type>
		{
			atom_type(std::string name);

			// Move
			atom_type(atom_type&& other);
			atom_type& operator=(atom_type&& other);

			// Copy
			atom_type(const atom_type& other);
			atom_type& operator=(const atom_type& other);

			std::string to_string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			std::string name;
		};

		struct any_type : public type, private comparable<any_type, type>, private copyable<any_type, type>
		{
			any_type();

			// Move
			any_type(any_type&& other);
			any_type& operator=(any_type&& other);

			// Copy
			any_type(const any_type& other);
			any_type& operator=(const any_type& other);

			std::string to_string() const override;
			bool operator==(type* other) const override { return true; }
			type* copy() const override { return copyable::copy(*this); }

			std::string name;
		};

		struct unset_type : public type, private comparable<unset_type, type>, private copyable<unset_type, type>
		{
			std::string to_string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }
		};

		// Composition types

		struct sum_type : public type, private comparable<sum_type, type>, private copyable<sum_type, type>
		{
			sum_type();
			sum_type(std::vector<unique_type> sum);

			// Move
			sum_type(sum_type&& other);
			sum_type& operator=(sum_type&& other);

			// Copy
			sum_type(const sum_type& other);
			sum_type& operator=(const sum_type& other);

			std::string to_string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			std::vector<unique_type> sum;
		};

		struct array_type : public type, private comparable<array_type, type>, private copyable<array_type, type>
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

			std::string to_string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			unique_type element_type;
		};

		struct reference_type : public type, private comparable<reference_type, type>, private copyable<reference_type, type>
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

			std::string to_string() const override;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			unique_type referred_type;
		};

		struct product_type : public type, private comparable<product_type, type>, private copyable<product_type, type>
		{
			product_type();
			product_type(std::vector<unique_type> product);

			// Move
			product_type(product_type&& other);
			product_type& operator=(product_type&& other);

			// Copy
			product_type(const product_type& other);
			product_type& operator=(const product_type& other);

			std::string to_string() const;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			std::vector<unique_type> product;
		};

		struct function_type : public type, private comparable<function_type, type>, private copyable<function_type, type>
		{
			function_type(unique_type f, unique_type t);
			function_type(const type& f, const type& t);

			// Move
			function_type(function_type&& other);
			function_type& operator=(function_type&& other);

			// Copy
			function_type(const function_type& other);
			function_type& operator=(const function_type& other);

			std::string to_string() const;
			bool operator==(type* other) const override { return comparable::operator==(other); }
			type* copy() const override { return copyable::copy(*this); }

			unique_type from, to;
		};


		// Helper methods

		const auto make_unique = [](auto& x) {
			return std::unique_ptr<type>(x.copy());
		};

		// Operators

		bool operator==(const atom_type& one, const atom_type& two);

		bool operator==(const unset_type& one, const unset_type& two);

		bool operator==(const sum_type& one, const sum_type& two);

		bool operator==(const product_type& one, const product_type& two);

		bool operator==(const function_type& one, const function_type& two);

		bool operator==(const array_type& one, const array_type& two);

		bool operator==(const reference_type& one, const reference_type& two);

		bool operator==(const any_type& one, const any_type& two);

		bool operator==(const atom_type& one, const atom_type& two);
	}
}
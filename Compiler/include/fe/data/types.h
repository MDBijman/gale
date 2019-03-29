#pragma once
#include <assert.h>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace fe
{
	namespace types
	{
		namespace detail
		{
			template <class Derived, class Base> struct comparable
			{
			      public:
				bool operator==(const Base *o) const
				{
					// This doesnt work with gcc, because
					// types::any is not defined yet, not
					// sure why it works with msvc
					// if (typeid(*o) == typeid(types::any))
					//	return true;

					if (typeid(Derived) != typeid(*o)) return false;

					const Derived &a = static_cast<const Derived &>(*this);
					const Derived &b = static_cast<const Derived &>(*o);

					return a == b;
				}
			};

			template <class Derived, class Base> struct copyable
			{
			      public:
				Base *copy(const Derived &o) const { return new Derived(o); }
			};
		} // namespace detail

		struct type
		{
			virtual ~type(){};
			virtual operator std::string() const = 0;
			virtual type *copy() const = 0;
			virtual size_t calculate_size() const = 0;
			virtual size_t calculate_offset(const std::vector<size_t> &offsets,
							size_t curr = 0) const = 0;
			virtual bool operator==(const type *other) const = 0;
		};

		using unique_type = std::unique_ptr<type>;

		// Atom types

		enum class atom_type
		{
			I8,
			I16,
			I32,
			I64,
			UI8,
			UI16,
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

		constexpr const char *atom_type_str(atom_type lit)
		{
			switch (lit)
			{
			case atom_type::I8: return "std.i8"; break;
			case atom_type::I16: return "std.i16"; break;
			case atom_type::I32: return "std.i32"; break;
			case atom_type::I64: return "std.i64"; break;
			case atom_type::UI8: return "std.ui8"; break;
			case atom_type::UI16: return "std.ui16"; break;
			case atom_type::UI32: return "std.ui32"; break;
			case atom_type::UI64: return "std.ui64"; break;
			case atom_type::F32: return "std.f32"; break;
			case atom_type::F64: return "std.f64"; break;
			case atom_type::STRING: return "std.str"; break;
			case atom_type::BOOL: return "std.bool"; break;
			case atom_type::UNSET: return "unset"; break;
			case atom_type::ANY: return "any"; break;
			case atom_type::VOID: return "void"; break;
			}
			// Throwing an error in a constexpr is not allowed by
			// gcc
			return "";
			// throw std::runtime_error("Unknown atom type");
		};

		constexpr size_t atom_type_size(atom_type lit)
		{
			switch (lit)
			{
			case atom_type::I8: return 1; break;
			case atom_type::I16: return 2; break;
			case atom_type::I32: return 4; break;
			case atom_type::I64: return 8; break;
			case atom_type::UI8: return 1; break;
			case atom_type::UI16: return 2; break;
			case atom_type::UI32: return 4; break;
			case atom_type::UI64: return 8; break;
			case atom_type::F32: return 4; break;
			case atom_type::F64: return 8; break;
			case atom_type::STRING: return 8; break;
			case atom_type::BOOL: return 1; break;
			case atom_type::UNSET: return -1; break;
			case atom_type::ANY: return -1; break;
			case atom_type::VOID: return 0; break;
			}
			// Throwing an error in a constexpr is not allowed by
			// gcc
			return 0;
			// throw std::runtime_error("Unknown atom type");
		}

		template <atom_type Type>
		struct atom : public type,
			      public detail::comparable<atom<Type>, type>,
			      public detail::copyable<atom<Type>, type>
		{
			atom() {}

			// Move
			atom(atom &&other) {}
			atom &operator=(atom &&other) {}

			// Copy
			atom(const atom &other) {}
			atom &operator=(const atom &other) {}

			operator std::string() const override { return atom_type_str(Type); }

			bool operator==(const type *other) const override
			{
				return detail::comparable<atom<Type>, type>::operator==(other);
			}
			type *copy() const override
			{
				return detail::copyable<atom<Type>, type>::copy(*this);
			}
			size_t calculate_size() const override { return atom_type_size(Type); }
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr == offsets.size());
				return 0;
			}
		};

		using i8 = atom<atom_type::I8>;
		using i16 = atom<atom_type::I16>;
		using i32 = atom<atom_type::I32>;
		using i64 = atom<atom_type::I64>;
		using ui8 = atom<atom_type::UI8>;
		using ui16 = atom<atom_type::UI16>;
		using ui32 = atom<atom_type::UI32>;
		using ui64 = atom<atom_type::UI64>;
		using f32 = atom<atom_type::F32>;
		using f64 = atom<atom_type::F64>;
		using boolean = atom<atom_type::BOOL>;
		using str = atom<atom_type::STRING>;
		using any = atom<atom_type::ANY>;
		using unset = atom<atom_type::UNSET>;
		using voidt = atom<atom_type::VOID>;

		// Composition types

		struct sum_type : public type,
				  public detail::comparable<sum_type, type>,
				  public detail::copyable<sum_type, type>
		{
			sum_type();
			sum_type(std::vector<unique_type> sum);

			// Move constructor
			sum_type(sum_type &&other);
			// Move assignment operator
			sum_type &operator=(sum_type &&other);

			// Copy constructor
			sum_type(const sum_type &other);
			// Copy assignment constructor
			sum_type &operator=(const sum_type &other);

			// Returns string representation of this type
			operator std::string() const override;

			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}

			type *copy() const override { return copyable::copy(*this); }

			size_t calculate_size() const override
			{
				size_t max = 0;
				for (auto &t : sum)
				{
					size_t s = t->calculate_size();
					max = s > max ? s : max;
				}
				return max;
			}

			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr == offsets.size());
				return 0;
			}

			types::type &operator[](int i) { return *sum[i]; }

			size_t index_of(std::string name);

			std::vector<unique_type> sum;
		};

		struct array_type : public type,
				    public detail::comparable<array_type, type>,
				    public detail::copyable<array_type, type>
		{
			array_type();
			array_type(unique_type t, size_t count);
			array_type(type &t, size_t count);

			// Move
			array_type(array_type &&other);
			array_type &operator=(array_type &&other);

			// Copy
			array_type(const array_type &other);
			array_type &operator=(const array_type &other);

			operator std::string() const override;
			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}
			type *copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				return count * element_type->calculate_size();
			}
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr == offsets.size());
				return 0;
			}

			size_t count;
			unique_type element_type;
		};

		struct reference_type : public type,
					public detail::comparable<reference_type, type>,
					public detail::copyable<reference_type, type>
		{
			reference_type();
			reference_type(unique_type t);
			reference_type(type &t);

			// Move
			reference_type(reference_type &&other);
			reference_type &operator=(reference_type &&other);

			// Copy
			reference_type(const reference_type &other);
			reference_type &operator=(const reference_type &other);

			operator std::string() const override;
			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}
			type *copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override { return 4; }
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr == offsets.size());
				return 0;
			}

			unique_type referred_type;
		};

		struct product_type : public type,
				      public detail::comparable<product_type, type>,
				      public detail::copyable<product_type, type>
		{
			product_type();
			product_type(std::vector<unique_type> product);

			// Move
			product_type(product_type &&other);
			product_type &operator=(product_type &&other);

			// Copy
			product_type(const product_type &other);
			product_type &operator=(const product_type &other);

			operator std::string() const override;
			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}
			type *copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override
			{
				size_t sum = 0;
				for (auto &t : product) sum += t->calculate_size();
				return sum;
			}
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr < offsets.size());
				auto offset = offsets[curr];
				size_t sum = 0;
				for (int i = 0; i < offset; i++)
					sum += product[i]->calculate_size();
				sum += product[offset]->calculate_offset(offsets, curr + 1);
				return sum;
			}

			std::vector<unique_type> product;
		};

		struct function_type : public type,
				       public detail::comparable<function_type, type>,
				       public detail::copyable<function_type, type>
		{
			function_type(unique_type f, unique_type t);
			function_type(const type &f, const type &t);

			// Move
			function_type(function_type &&other);
			function_type &operator=(function_type &&other);

			// Copy
			function_type(const function_type &other);
			function_type &operator=(const function_type &other);

			operator std::string() const override;
			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}
			type *copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override { return to->calculate_size(); }
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				assert(curr == offsets.size());
				return 0;
			}

			unique_type from, to;
		};

		struct nominal_type : public type,
				      public detail::comparable<nominal_type, type>,
				      public detail::copyable<nominal_type, type>
		{
			nominal_type(std::string name, unique_type inner);
			nominal_type(std::string name, const type &inner);

			// Move
			nominal_type(nominal_type &&other);
			nominal_type &operator=(nominal_type &&other);

			// Copy
			nominal_type(const nominal_type &other);
			nominal_type &operator=(const nominal_type &other);

			operator std::string() const override;
			bool operator==(const type *other) const override
			{
				return comparable::operator==(other);
			}
			type *copy() const override { return copyable::copy(*this); }
			size_t calculate_size() const override { return inner->calculate_size(); }
			size_t calculate_offset(const std::vector<size_t> &offsets,
						size_t curr = 0) const override
			{
				return inner->calculate_offset(offsets, curr);
			}

			unique_type inner;
			std::string name;
		};

		// Helper methods

		const auto make_unique = [](auto &x) { return std::unique_ptr<type>(x.copy()); };

		// Operators

		template <atom_type T> bool operator==(const atom<T> &one, const atom<T> &two)
		{
			return true;
		}

		bool operator==(const sum_type &one, const sum_type &two);

		bool operator==(const product_type &one, const product_type &two);

		bool operator==(const function_type &one, const function_type &two);

		bool operator==(const array_type &one, const array_type &two);

		bool operator==(const reference_type &one, const reference_type &two);

		bool operator==(const nominal_type &one, const nominal_type &two);
	} // namespace types
} // namespace fe
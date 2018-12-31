#pragma once
#include <variant>
#include <vector>
#include <assert.h>


namespace memory
{
	template<typename T, int N>
	class small_vector
	{
		// if 0 <= stack_size <= N then content is stack local
		size_t size_ = 0;
		union data
		{
			data() : stack_data() {}
			T stack_data[N];
			struct
			{
				T* heap_data;
				size_t space;
			};
		} content;

		bool on_stack()
		{
			return size_ <= N;
		}

		void move_to_heap()
		{
			assert(on_stack());

			auto new_data = new T[size_ * 2];
			for (size_t i = 0; i < N; i++)
				new_data[i] = std::move(content.stack_data[i]);
			content.heap_data = new_data;
			content.space = size_ * 2;
		}

		void grow_on_heap()
		{
			assert(!on_stack());

			auto new_space = content.space * 2;
			auto new_data = new T[new_space];
			for (size_t i = 0; i < size_; i++)
				new_data[i] = std::move(content.heap_data[i]);
			delete[] content.heap_data;
			content.heap_data = new_data;
			content.space = new_space;
		}

	public:
		small_vector() : content() {}
		small_vector(const small_vector<T, N>& o) : size_(o.size_)
		{
			if (size_ <= N)
			{
				for (int i = 0; i < N; i++)
					content.stack_data[i] = o.content.stack_data[i];
			}
			else
			{
				content.space = o.content.space;
				content.heap_data = new T[content.space];
				for (int i = 0; i < size_; i++)
					content.heap_data[i] = o.content.heap_data[i];
			}
		}
		~small_vector()
		{
			if (!on_stack())
			{
				delete[] content.heap_data;
			}
		}

		size_t size()
		{
			return size_;
		}

		void push_back(T t)
		{
			if (size_ < N)
			{
				content.stack_data[size_] = std::move(t);
				size_++;
				return;
			}

			if (size_ == N)
			{
				move_to_heap();
			}

			// on heap

			if (size_ + 1 == content.space)
			{
				grow_on_heap();
			}

			content.heap_data[size_] = std::move(t);
			size_++;
		}

		T& at(size_t i)
		{
			if (on_stack())
			{
				return content.stack_data[i];
			}
			else
			{
				return content.heap_data[i];
			}
		}

		T& operator[](size_t i)
		{
			return at(i);
		}

		T& back()
		{
			assert(size_ > 0);
			return at(size_ - 1);
		}

		using iterator = T*;
		using reverse_iterator = std::reverse_iterator<T*>;

		iterator begin()
		{
			if (on_stack())
				return &content.stack_data[0];
			else
				return &content.heap_data[0];
		}

		iterator end()
		{
			if (on_stack())
				return &content.stack_data[size_];
			else
				return &content.heap_data[size_];
		}

		reverse_iterator rbegin()
		{
			return std::make_reverse_iterator(end());
		}

		reverse_iterator rend()
		{
			return std::make_reverse_iterator(begin());
		}
	};
}
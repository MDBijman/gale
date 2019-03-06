#pragma once
#include "ast_data.h"
#include "utils/memory/data_store.h"

namespace fe
{
	struct constants_store;

	/*
	* We have to do this because gcc doesnt compile template specializations within a struct/class.
	*/
	namespace detail
	{
		template<class DataType> DataType& get(constants_store& cs, data_index_t i);
		template<class DataType> data_index_t create(constants_store& cs);
	}

	struct constants_store
	{
	public:
		memory::dynamic_store<plain_identifier> identifiers;
		memory::dynamic_store<boolean> booleans;
		memory::dynamic_store<string> strings;
		memory::dynamic_store<number> numbers;

		// Node data 
		template<class DataType>
		DataType& get(data_index_t i)
		{
			return detail::get<DataType>(*this, i);
		}

		// Node data 
		template<class DataType>
		data_index_t create()
		{
			return detail::create<DataType>(*this);
		}
	};

	namespace detail
	{
		template<> plain_identifier& get<plain_identifier>(constants_store& cs, data_index_t i);
		template<> boolean& get<boolean>(constants_store& cs, data_index_t i);
		template<> string& get<string>(constants_store& cs, data_index_t i);
		template<> number& get<number>(constants_store& cs, data_index_t i);

		template<> data_index_t create<plain_identifier>(constants_store& cs);
		template<> data_index_t create<boolean>(constants_store& cs);
		template<> data_index_t create<string>(constants_store& cs);
		template<> data_index_t create<number>(constants_store& cs);
	}
}
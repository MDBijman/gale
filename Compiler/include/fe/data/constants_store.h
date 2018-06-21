#pragma once
#include "ast_data.h"
#include "utils/memory/data_store.h"

namespace fe
{
	struct constants_store
	{
	public:
		memory::dynamic_store<plain_identifier> identifiers;
		memory::dynamic_store<boolean> booleans;
		memory::dynamic_store<string> strings;
		memory::dynamic_store<number> numbers;

		// Node data 
		template<class DataType>
		DataType& get(data_index i);
		template<> plain_identifier& get<plain_identifier>(data_index i) { return identifiers.get_at(i); }
		template<> boolean& get<boolean>(data_index i) { return booleans.get_at(i); }
		template<> string& get<string>(data_index i) { return strings.get_at(i); }
		template<> number& get<number>(data_index i) { return numbers.get_at(i); }

		// Node data 
		template<class DataType>
		data_index create();
		template<> data_index create<plain_identifier>() { return identifiers.create(); }
		template<> data_index create<boolean>() { return booleans.create(); }
		template<> data_index create<string>() { return strings.create(); }
		template<> data_index create<number>() { return numbers.create(); }
	};
}
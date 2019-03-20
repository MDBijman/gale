#include "fe/data/constants_store.h"

namespace fe
{
	namespace detail
	{
		template<> plain_identifier& get<plain_identifier>(constants_store& cs, data_index_t i) { return cs.identifiers.get_at(i); }
		template<> boolean& get<boolean>(constants_store& cs, data_index_t i) { return cs.booleans.get_at(i); }
		template<> string& get<string>(constants_store& cs, data_index_t i) { return cs.strings.get_at(i); }
		template<> number& get<number>(constants_store& cs, data_index_t i) { return cs.numbers.get_at(i); }

		template<> data_index_t create<plain_identifier>(constants_store& cs) { return static_cast<data_index_t>(cs.identifiers.create()); }
		template<> data_index_t create<boolean>(constants_store& cs) { return static_cast<data_index_t>(cs.booleans.create()); }
		template<> data_index_t create<string>(constants_store& cs) { return static_cast<data_index_t>(cs.strings.create()); }
		template<> data_index_t create<number>(constants_store& cs) { return static_cast<data_index_t>(cs.numbers.create()); }
	}
}
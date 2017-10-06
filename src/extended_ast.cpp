#include "extended_ast.h"

namespace fe
{
	namespace extended_ast
	{
		function_call::function_call(const function_call& other) : id(other.id), params(new node(*other.params)), type(other.type), tags(other.tags) {}

		assignment::assignment(const assignment& other) : id(other.id), value(new node(*other.value)), type(other.type), tags(other.tags) {}

		function::function(const function& other) : name(other.name), from(other.from), to(new node(*other.to)), body(new node(*other.body)), tags(other.tags) {}

		conditional_branch_path::conditional_branch_path(const conditional_branch_path& other) : test_path(new node(*other.test_path)), code_path(new node(*other.code_path)), type(other.type), tags(other.tags) {}
	}
}
#include "fe/language_definition.h"

namespace fe
{
	namespace non_terminals
	{
		recursive_descent::non_terminal
			file, statement, export_stmt, declaration, expression, value_tuple,
			function, match, operation, term, addition, subtraction,
			multiplication, division, brackets, array_index, index, module_imports,
			match_branch, type_expression, type_tuple,
			function_type, type_definition, module_declaration,
			block, function_call, record, record_element,
			type_atom, reference_type, array_type, reference, array_value, while_loop,
			arithmetic, equality, type_operation, type_modifiers, assignable, identifier_tuple,
			assignment, greater_than, modulo, less_or_equal, comparison, greater_or_equal, less_than,
			if_expr, stmt_semicln, block_elements, block_result, elseif_expr, else_expr,
			logical, and_expr, or_expr
			;
	}
}

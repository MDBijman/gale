#include "fe/pipeline/pretty_print_stage.h"
#include "fe/data/ext_ast.h"

#include <stack>

namespace fe::ext_ast
{
	std::string to_string(node_type t)
	{
		switch (t)
		{
		case node_type::ASSIGNMENT:        return "Assignment";
		case node_type::TUPLE:             return "Tuple";
		case node_type::BLOCK:             return "Block";
		case node_type::BLOCK_RESULT:      return "BlockResult";
		case node_type::FUNCTION:          return "Function";
		case node_type::WHILE_LOOP:        return "While";
		case node_type::IF_STATEMENT:      return "If";
		case node_type::ELSEIF_STATEMENT:  return "ElseIf";
		case node_type::ELSE_STATEMENT:    return "Else";
		case node_type::MATCH_BRANCH:      return "MatchBranch";
		case node_type::MATCH:             return "Match";
		case node_type::IDENTIFIER:        return "Id";
		case node_type::FUNCTION_CALL:     return "Call";
		case node_type::MODULE_DECLARATION:return "ModuleDeclaration";
		case node_type::EXPORT_STMT:       return "Export";
		case node_type::IMPORT_DECLARATION:return "Import";
		case node_type::DECLARATION:       return "Declaration";
		case node_type::REFERENCE:         return "Reference";
		case node_type::ARRAY_VALUE:       return "ArrayValue";
		case node_type::STRING:            return "String";
		case node_type::BOOLEAN:           return "Boolean";
		case node_type::NUMBER:            return "Number";
		case node_type::TYPE_DEFINITION:   return "TypeDefinition";
		case node_type::IDENTIFIER_TUPLE:  return "IdentifierTuple";
		case node_type::TUPLE_TYPE:        return "TupleType";
		case node_type::ATOM_TYPE:         return "AtomType";
		case node_type::FUNCTION_TYPE:     return "FunctionType";
		case node_type::REFERENCE_TYPE:    return "ReferenceType";
		case node_type::ARRAY_TYPE:        return "ArrayType";
		case node_type::SUM_TYPE:          return "SumType";
		case node_type::AND:               return "And";
		case node_type::OR:                return "Or";
		case node_type::NOT:               return "Not";
		case node_type::ADDITION:          return "Add";
		case node_type::SUBTRACTION:       return "Sub";
		case node_type::MULTIPLICATION:    return "Mul";
		case node_type::DIVISION:          return "Div";
		case node_type::MODULO:            return "Mod";
		case node_type::EQUALITY:          return "Eq";
		case node_type::GREATER_THAN:      return "Gt";
		case node_type::GREATER_OR_EQ:     return "Ge";
		case node_type::LESS_THAN:         return "Lt";
		case node_type::LESS_OR_EQ:        return "Le";
		case node_type::ARRAY_ACCESS:      return "ArrayAccess";
		default: throw std::runtime_error("Cannot pretty print node, unknown type");
		}
	}

	std::string data_to_string(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::IDENTIFIER: return "\"" + ast.get_data<ext_ast::identifier>(n.data_index).full + "\"";
		case node_type::NUMBER: return std::to_string(ast.get_data<number>(n.data_index).value);
		case node_type::BOOLEAN: return std::to_string(ast.get_data<boolean>(n.data_index).value);
		default: return "";
		}

	}

	void pretty_print(std::string& acc, node_id curr, ast& ast)
	{
		auto& node = ast.get_node(curr);

		acc += to_string(node.kind);
		acc += "(";
		if (node.children_id != no_children)
		{
			for (auto child : ast.get_children(node.children_id))
			{
				pretty_print(acc, child, ast);
				acc += ", ";
			}

			// Remove the ', ' after the last element
			acc.pop_back();
			acc.pop_back();
		}
		else
		{
			acc += data_to_string(node, ast);
		}
		acc += ")";
	}

	std::string pretty_print(ast& ast)
	{
		std::string out;
		pretty_print(out, ast.root_id(), ast);
		return out;
	}
}

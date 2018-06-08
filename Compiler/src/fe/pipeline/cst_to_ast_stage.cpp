#include "fe/pipeline/cst_to_ast_stage.h"
#include "utils/parsing/ebnfe_parser.h"
#include "fe/language_definition.h"
#include "utils/algorithms/tree_traversal.h"

namespace fe
{
	ext_ast::node_type nt_to_node_type(utils::ebnfe::non_terminal nt)
	{
		using namespace ext_ast;
		if      (nt == non_terminals::file)                       return node_type::BLOCK;
		else if (nt == non_terminals::module_declaration)         return node_type::MODULE_DECLARATION;
		else if (nt == non_terminals::declaration)                return node_type::DECLARATION;
		else if (nt == non_terminals::assignment)                 return node_type::ASSIGNMENT;
		else if (nt == non_terminals::function_call)              return node_type::FUNCTION_CALL;
		else if (nt == non_terminals::block)                      return node_type::BLOCK;
		else if (nt == non_terminals::value_tuple)                return node_type::TUPLE;
		else if (nt == non_terminals::type_definition)            return node_type::TYPE_DEFINITION;
		else if (nt == non_terminals::record)                     return node_type::RECORD;
		else if (nt == non_terminals::record_element)             return node_type::RECORD_ELEMENT;
		else if (nt == non_terminals::type_tuple)                 return node_type::TYPE_TUPLE;
		else if (nt == non_terminals::function_type)              return node_type::FUNCTION_TYPE;
		else if (nt == non_terminals::type_atom)                  return node_type::TYPE_ATOM;
		else if (nt == non_terminals::function)                   return node_type::FUNCTION;
		else if (nt == non_terminals::match)                      return node_type::MATCH;
		else if (nt == non_terminals::match_branch)               return node_type::MATCH_BRANCH;
		else if (nt == non_terminals::reference_type)             return node_type::REFERENCE_TYPE;
		else if (nt == non_terminals::array_type)                 return node_type::ARRAY_TYPE;
		else if (nt == non_terminals::reference)                  return node_type::REFERENCE;
		else if (nt == non_terminals::array_value)                return node_type::ARRAY_VALUE;
		else if (nt == non_terminals::addition)                   return node_type::ADDITION;
		else if (nt == non_terminals::subtraction)                return node_type::SUBTRACTION;
		else if (nt == non_terminals::multiplication)             return node_type::MULTIPLICATION;
		else if (nt == non_terminals::division)                   return node_type::DIVISION;
		else if (nt == non_terminals::module_declaration)         return node_type::MODULE_DECLARATION;
		else if (nt == non_terminals::module_imports)             return node_type::IMPORT_DECLARATION;
		else if (nt == non_terminals::while_loop)                 return node_type::WHILE_LOOP;
		else if (nt == non_terminals::equality)                   return node_type::EQUALITY;
		else if (nt == non_terminals::identifier_tuple)           return node_type::IDENTIFIER_TUPLE;
		else if (nt == non_terminals::greater_than)               return node_type::GREATER_THAN;
		else if (nt == non_terminals::less_or_equal)              return node_type::LESS_OR_EQ;
		else if (nt == non_terminals::greater_or_equal)           return node_type::GREATER_OR_EQ;
		else if (nt == non_terminals::less_than)                  return node_type::LESS_THAN;
		else if (nt == non_terminals::modulo)                     return node_type::MODULO;
		else if (nt == non_terminals::if_expr)                    return node_type::IF_STATEMENT;
		else if (nt == non_terminals::elseif_expr)                return node_type::IF_STATEMENT;
		else if (nt == non_terminals::else_expr)                  return node_type::IF_STATEMENT;
		else if (nt == non_terminals::block_result)               return node_type::BLOCK_RESULT;
		else if (nt == non_terminals::and_expr)                   return node_type::AND;
		else if (nt == non_terminals::or_expr)                    return node_type::OR;
		assert(!"Unknown non-terminal");
		throw std::runtime_error("Unknown non-terminal");
	}

	ext_ast::node_type t_to_node_type(utils::ebnfe::terminal t)
	{
		using namespace ext_ast;
		if (t == terminals::number)           return node_type::NUMBER;
		else if (t == terminals::word)        return node_type::STRING;
		else if (t == terminals::true_keyword
			|| t == terminals::false_keyword) return node_type::BOOLEAN;
		else if (t == terminals::identifier)  return node_type::IDENTIFIER;
		assert(!"Unknown terminal");
		throw std::runtime_error("Unknown terminal");
	}

	ext_ast::ast cst_to_ast_stage::conv(utils::bnf::tree in) const
	{
		// Post order traversal of the bnf tree to construct cst.
		using namespace utils;
		auto order = algorithms::post_order(in);

		ext_ast::ast new_tree;
		std::vector<bnf::node_id> converted;

		for(auto id : order)
		{
			auto& node = in.get_node(id);

			if(node.kind == bnf::node_type::NON_TERMINAL)
			{
				auto& data = in.get_non_terminal(node.value_id);

				auto new_node = new_tree.create_node(nt_to_node_type(data.first));

				std::vector<bnf::node_id> children;
				for (int i = data.second.size(); i > 0; i--)
				{
					auto child = converted.at(converted.size() - i);
					children.push_back(child);
					new_tree.get_node(child).parent_id = new_node;
				}

				converted.resize(converted.size() - data.second.size());
				converted.push_back(new_node);
				new_tree.get_node(new_node).children = std::move(children);
				new_tree.set_root_id(new_node);
			}
			// If we have a terminal node, we put it onto the converted stack after creating the proper node
			else
			{
				auto& data = in.get_terminal(node.value_id);
				auto id = new_tree.create_node(t_to_node_type(data.first));
				auto& node = new_tree.get_node(id);
				auto data_id = node.data_index.value();

				if (data.first == terminals::number)
					new_tree.get_data<number>(data_id).value = std::stoll(data.second);

				else if (data.first == terminals::word)
					new_tree.get_data<string>(data_id).value = data.second.substr(1, data.second.size() - 2);

				else if (data.first == terminals::true_keyword)
					new_tree.get_data<boolean>(data_id).value = true;

				else if (data.first == terminals::false_keyword)
					new_tree.get_data<boolean>(data_id).value = false;

				else if (data.first == terminals::identifier)
				{
					auto split_on = [](std::string identifier, char split_on) -> std::vector<std::string> {
						std::vector<std::string> split_identifier;

						std::string::iterator begin_word = identifier.begin();
						for (auto it = identifier.begin(); it != identifier.end(); it++)
						{
							if (*it == split_on)
							{
								// Read infix
								split_identifier.push_back(std::string(begin_word, it));
								begin_word = it + 1;
								continue;
							}
							else
							{
								if (it == identifier.end() - 1)
									split_identifier.push_back(std::string(begin_word, it + 1));
							}
						}
						return split_identifier;
					};

					auto splitted = split_on(data.second, '.');

					new_tree.get_data<ext_ast::identifier>(data_id).segments = splitted;
				}

				converted.push_back(id);
			}
		}
		return new_tree;
	}
}
#include "fe/pipeline/interface_extraction_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/data/interface.h"
#include "fe/data/ext_ast.h"

namespace fe::ext_ast
{
	fe::interface extract_interface(ast &ast)
	{
		fe::interface res;
		auto helper = ast_helper(ast);

		res.name = ast.get_module_name()->name;

		auto& import_identifiers = ast.get_imports();
		if(import_identifiers)
		{
			for(auto& id : *import_identifiers)
				res.imports.push_back(id.full);
		}

		ast_helper(ast).for_all_t(node_type::FUNCTION, [&ast, &res](node &n) {
			n.type_scope_id = ast[n.parent_id].type_scope_id;
			auto &children = ast.children_of(n);
			auto &lhs_node = ast[children[0]];
			auto &type_node = ast[children[1]];

			auto &id_data = ast.get_data<identifier>(lhs_node.data_index);
			auto fun_type = typeof(type_node, ast);

			res.names.declare_variable(id_data.name, lhs_node.id);
			res.names.define_variable(id_data.name);
			res.types.set_type(id_data.name, std::move(fun_type));
		});

		return res;
	}
} // namespace fe::ext_ast